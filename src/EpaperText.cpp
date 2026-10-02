#include "Epaper213.h"
#include "fonts/EpaperFonts.h"

#include <stdio.h>

namespace {
	uint16_t nextCodepoint(const char *&text) {
		const uint8_t first = static_cast<uint8_t>(*text++);
		if (first < 0x80) return first;
		if (first >= 0xc2 && first <= 0xdf) {
			const uint8_t second = static_cast<uint8_t>(*text);
			if ((second & 0xc0) == 0x80) {
				++text;
				return (static_cast<uint16_t>(first & 0x1f) << 6) | (second & 0x3f);
			}
		} else if (first >= 0xe0 && first <= 0xef) {
			// Consume valid three-byte UTF-8 as a single unsupported character.
			const uint8_t second = static_cast<uint8_t>(text[0]);
			if (second && (second & 0xc0) == 0x80) {
				++text;
				const uint8_t third = static_cast<uint8_t>(*text);
				if (third && (third & 0xc0) == 0x80) ++text;
			}
		} else if (first >= 0xf0 && first <= 0xf4) {
			// Unsupported four-byte characters (e.g. emoji) become one '?'.
			for (uint8_t i = 0; i < 3 && *text &&
				(static_cast<uint8_t>(*text) & 0xc0) == 0x80; ++i) ++text;
		}
		return '?';
	}

	const EpaperFonts::Font &fontFor(EpaperFont font) {
		switch (font) {
			case EpaperFont::Medium: return EpaperFonts::Medium;
			case EpaperFont::Large: return EpaperFonts::Large;
			case EpaperFont::MediumBold: return EpaperFonts::MediumBold;
			case EpaperFont::LargeBold: return EpaperFonts::LargeBold;
			case EpaperFont::ExtraLargeBold: return EpaperFonts::ExtraLargeBold;
			case EpaperFont::ClockBold: return EpaperFonts::ClockBold;
			default: return EpaperFonts::Small;
		}
	}
	const uint8_t *glyphFor(uint16_t codepoint, const EpaperFonts::Font &font) {
		size_t index = 0;
		for (size_t i = 0; i < font.count; ++i) {
			if (font.codepoints[i] == '?') index = i;
		}
		for (size_t i = 0; i < font.count; ++i) {
			if (font.codepoints[i] == codepoint) { index = i; break; }
		}
		return font.data + index * font.height * font.rowBytes;
	}
}

void Epaper213::drawText(uint16_t x, uint16_t y, const char *text, uint8_t scale, bool white) {
	drawTextScaled(x, y, text, EpaperFont::Small, scale, white);
}

void Epaper213::drawText(uint16_t x, uint16_t y, const char *text, EpaperFont font, bool white) {
	drawTextScaled(x, y, text, font, 1, white);
}

void Epaper213::drawTextScaled(uint16_t x, uint16_t y, const char *text, EpaperFont selected, uint8_t scale, bool white) {
	if (!text || !scale || x >= WIDTH || y >= HEIGHT) return;
	const auto &font = fontFor(selected);
	const uint32_t left = x;
	uint32_t penX = x;
	uint32_t penY = y;
	while (*text) {
		uint16_t codepoint = nextCodepoint(text);
		if (codepoint == '\r') continue;
		if (codepoint == '\n') {
			penX = left;
			penY += static_cast<uint32_t>(font.height + 2) * scale;
			if (penY >= HEIGHT) break;
			continue;
		}
		if (penX >= WIDTH) break;
		const uint8_t *glyph = glyphFor(codepoint, font);
		for (uint8_t row = 0; row < font.height; ++row) {
			for (uint8_t col = 0; col < font.width; ++col) {
				if (glyph[row * font.rowBytes + col / 8] & (0x80 >> (col % 8))) {
					uint32_t px = penX + static_cast<uint32_t>(col) * scale;
					uint32_t py = penY + static_cast<uint32_t>(row) * scale;
					if (px < WIDTH && py < HEIGHT)
						fillRect(px, py, scale, scale, white);
				}
			}
		}
		penX += static_cast<uint32_t>(font.advance) * scale;
	}
}

void Epaper213::drawNumber(uint16_t x, uint16_t y, int32_t number, uint8_t scale, bool white) {
	char digits[12];
	snprintf(digits, sizeof(digits), "%ld", static_cast<long>(number));
	drawText(x, y, digits, scale, white);
}

void Epaper213::drawNumber(uint16_t x, uint16_t y, int32_t number, EpaperFont font, bool white) {
	char digits[12];
	snprintf(digits, sizeof(digits), "%ld", static_cast<long>(number));
	drawText(x, y, digits, font, white);
}

uint8_t Epaper213::fontHeight(EpaperFont font) { return fontFor(font).height; }

uint16_t Epaper213::textWidth(const char *text, EpaperFont selected) {
	if (!text) return 0;
	const auto &font = fontFor(selected);
	uint32_t width = 0, maximum = 0;
	while (*text) {
		const uint16_t cp = nextCodepoint(text);
		if (cp == '\r') continue;
		if (cp == '\n') {
			if (width > maximum) maximum = width;
			width = 0;
		} else if (width < 65535) width += font.advance;
	}
	if (width > maximum) maximum = width;
	return maximum > 65535 ? 65535 : static_cast<uint16_t>(maximum);
}