#include "Epaper213.h"
#include <string.h>

namespace {
	const SPISettings spiConfig(4000000, MSBFIRST, SPI_MODE0);
}

void Epaper213::command(uint8_t value) {
	SPI.beginTransaction(spiConfig);
	digitalWrite(EpaperPins::DC, LOW);
	digitalWrite(EpaperPins::CS, LOW);
	SPI.transfer(value);
	digitalWrite(EpaperPins::CS, HIGH);
	SPI.endTransaction();
}

void Epaper213::data(uint8_t value) {
	SPI.beginTransaction(spiConfig);
	digitalWrite(EpaperPins::DC, HIGH);
	digitalWrite(EpaperPins::CS, LOW);
	SPI.transfer(value);
	digitalWrite(EpaperPins::CS, HIGH);
	SPI.endTransaction();
}

bool Epaper213::busy() {
	uint32_t start = millis();
	while (digitalRead(EpaperPins::BUSY) == HIGH) {
		if (millis() - start > 15000) {
			Serial.println("EPD busy timeout");
			ready_ = false;
			return false;
		}
		delay(10);
	}
	delay(10);
	Serial.println("EPD busy done");
	return true;
}

void Epaper213::window(uint8_t xs, uint8_t xe, uint16_t ys, uint16_t ye) {
	command(0x44);
	data(xs);
	data(xe);
	command(0x45);
	data(ys & 255);
	data(ys >> 8);
	data(ye & 255);
	data(ye >> 8);
	command(0x4e);
	data(xs);
	command(0x4f);
	data(ys & 255);
	data(ys >> 8);
}

void Epaper213::write(uint8_t cmd, uint8_t xs, uint8_t xe, uint16_t ys, uint16_t ye) {
	window(xs, xe, ys, ye);
	command(cmd);
	SPI.beginTransaction(spiConfig);
	digitalWrite(EpaperPins::DC, HIGH);
	digitalWrite(EpaperPins::CS, LOW);
	for (uint16_t row = ys; row <= ye; ++row)
		for (uint8_t col = xs; col <= xe; ++col)
			SPI.transfer(buffer_[static_cast<size_t>(row) * ROW_BYTES + col]);
	digitalWrite(EpaperPins::CS, HIGH);
	SPI.endTransaction();
}

bool Epaper213::update(uint8_t mode) {
	command(0x22);
	data(mode);
	command(0x20);
	return busy();
}

bool Epaper213::begin() {
	Serial.println("EPD init");
	pinMode(EpaperPins::CS, OUTPUT);
	pinMode(EpaperPins::DC, OUTPUT);
	pinMode(EpaperPins::RST, OUTPUT);
	pinMode(EpaperPins::BUSY, INPUT);
	digitalWrite(EpaperPins::CS, HIGH);
	SPI.begin(EpaperPins::SCK, -1, EpaperPins::MOSI, -1);
	Serial.println("EPD reset");
	digitalWrite(EpaperPins::RST, HIGH);
	delay(20);
	digitalWrite(EpaperPins::RST, LOW);
	delay(2);
	digitalWrite(EpaperPins::RST, HIGH);
	delay(20);
	if (!busy()) return false;
	command(0x12);
	if (!busy()) return false;
	command(0x01);
	data(0xf9);
	data(0x00);
	data(0x00);
	command(0x11);
	data(0x03);
	window(0, 15, 0, WIDTH - 1);
	command(0x3c);
	data(0x05);
	command(0x21);
	data(0x00);
	data(0x80);
	command(0x18);
	data(0x80);
	if (!busy()) return false;
	clear();
	ready_ = true;
	base_ = false;
	partial_ = false;
	return true;
}

void Epaper213::clear(bool white) {
	memset(buffer_, white ? 255 : 0, BUFFER_SIZE);
	for (uint16_t x = 0; x < WIDTH; ++x) buffer_[x * ROW_BYTES + 15] |= 0x3f;
}

void Epaper213::setPixel(uint16_t x, uint16_t y, bool white) {
	if (x >= WIDTH || y >= HEIGHT) return;
	// Visible controller X runs opposite to the logical top-to-bottom axis.
	// Mirror only 122 visible pixels, not the 128-bit padded RAM row.
	const uint16_t ramX = HEIGHT - 1 - y;
	uint8_t &byte = buffer_[static_cast<size_t>(x) * ROW_BYTES + ramX / 8];
	uint8_t mask = 0x80 >> (ramX % 8);
	if (white) byte |= mask;
	else byte &= static_cast<uint8_t>(~mask);
}

void Epaper213::fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool white) {
	if (x >= WIDTH || y >= HEIGHT || !w || !h) return;
	uint16_t xe = static_cast<uint32_t>(x) + w > WIDTH ? WIDTH : x + w;
	uint16_t ye = static_cast<uint32_t>(y) + h > HEIGHT ? HEIGHT : y + h;
	for (uint16_t i = x; i < xe; ++i)
		for (uint16_t j = y; j < ye; ++j) setPixel(i, j, white);
}

bool Epaper213::restoreBaseImage() {
	if (!ready_) return false;
	write(0x24, 0, 15, 0, WIDTH - 1);
	write(0x26, 0, 15, 0, WIDTH - 1);
	base_ = busy();
	return base_;
}

bool Epaper213::fullRefresh() {
	if (!ready_) return false;
	Serial.println("EPD full refresh");
	if (partial_) {
		command(0x3c);
		data(0x05);
		partial_ = false;
	}
	write(0x24, 0, 15, 0, WIDTH - 1);
	write(0x26, 0, 15, 0, WIDTH - 1);
	base_ = update(0xf7);
	return base_;
}

bool Epaper213::partialRefresh(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
	if (!ready_ || !base_ || x >= WIDTH || y >= HEIGHT || !w || !h) return false;
	uint16_t xe = static_cast<uint32_t>(x) + w > WIDTH ? WIDTH : x + w;
	uint16_t ye = static_cast<uint32_t>(y) + h > HEIGHT ? HEIGHT : y + h;
	Serial.printf("EPD partial refresh x=%u y=%u w=%u h=%u\n", x, y, xe - x, ye - y);
	if (!partial_) {
		command(0x3c);
		data(0x80);
		partial_ = true;
	}
	// Transform the clipped half-open interval [y, ye) before byte alignment.
	uint8_t first = (HEIGHT - ye) / 8;
	uint8_t last = (HEIGHT - 1 - y) / 8;
	write(0x24, first, last, x, xe - 1);
	if (!update(0xff)) return false;
	write(0x26, first, last, x, xe - 1);
	return true;
}

void Epaper213::sleep() {
	if (!ready_) return;
	command(0x10);
	data(0x01);
	delay(100);
	ready_ = false;
	base_ = false;
}