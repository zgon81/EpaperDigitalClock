#pragma once
#include <Arduino.h>
#include <SPI.h>

namespace EpaperPins {
	constexpr int CS = D1;
	constexpr int DC = D3;
	constexpr int RST = D0;
	constexpr int BUSY = D2;
	constexpr int SCK = D8;
	constexpr int MOSI = D10;
}

enum class EpaperFont : uint8_t { Small, Medium, Large, MediumBold, LargeBold, ExtraLargeBold, ClockBold };

class Epaper213 {
public:
	static constexpr uint16_t WIDTH = 250;
	static constexpr uint16_t HEIGHT = 122;
	static constexpr uint16_t ROW_BYTES = 16;
	static constexpr size_t BUFFER_SIZE = WIDTH * ROW_BYTES;
	bool begin();
	void clear(bool white = true);
	void setPixel(uint16_t x, uint16_t y, bool white);
	void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool white);
	// UTF-8, top-left cell origin. Legacy scale overload uses Small.
	void drawText(uint16_t x, uint16_t y, const char *text, uint8_t scale = 1, bool white = false);
	void drawNumber(uint16_t x, uint16_t y, int32_t number, uint8_t scale = 1, bool white = false);
	void drawText(uint16_t x, uint16_t y, const char *text, EpaperFont font, bool white = false);
	void drawNumber(uint16_t x, uint16_t y, int32_t number, EpaperFont font, bool white = false);
	static uint16_t textWidth(const char *text, EpaperFont font = EpaperFont::Small);
	static uint8_t fontHeight(EpaperFont font = EpaperFont::Small);
	bool fullRefresh();
	bool partialRefresh(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
	void sleep();
private:
	void drawTextScaled(uint16_t x, uint16_t y, const char *text, EpaperFont font, uint8_t scale, bool white);
	uint8_t buffer_[BUFFER_SIZE] = {};
	bool ready_ = false;
	bool base_ = false;
	bool partial_ = false;
	void command(uint8_t value);
	void data(uint8_t value);
	bool busy();
	void window(uint8_t xs, uint8_t xe, uint16_t ys, uint16_t ye);
	void write(uint8_t cmd, uint8_t xs, uint8_t xe, uint16_t ys, uint16_t ye);
	bool update(uint8_t mode);
};