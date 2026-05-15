#include "text_input.h"

#include <stddef.h>

#include "../gfx/gfx.h"
#include "../gfx/font.h"
#include "../kmalloc.h"
#include "../memory.h"
#include "../string.h"
#include "../time.h"
#include "../console.h"

#include "../drivers/usb/usb_hid_keyboard.h"
#include "../drivers/usb/usb_hid_keys.h"

#define TEXT_INPUT_INITIAL_CAPACITY 128
#define TEXT_INPUT_CHAR_WIDTH 8

int text_input_alloc(text_input_t *text_input) {
    memory_set(text_input, 0, sizeof(text_input_t));

    text_input->text = (char *)kmalloc(TEXT_INPUT_INITIAL_CAPACITY);
    if (text_input->text == NULL) {
        return -1;
    }
    memory_set(text_input->text, 0, TEXT_INPUT_INITIAL_CAPACITY);

    text_input->length = 0;
    text_input->capacity = TEXT_INPUT_INITIAL_CAPACITY;
    text_input->cursor_index = 0;

    text_input->position.x = 0;
    text_input->position.y = 0;
    text_input->cursor_color = RGB_COLOR_WHITE;
    text_input->text_color = RGB_COLOR_WHITE;

    text_input->cursor_size.x = TEXT_INPUT_CHAR_WIDTH;
    text_input->cursor_size.y = font_get_line_height();

    return 0;
}

int text_input_free(text_input_t *text_input) {
    kfree(text_input->text);
    memory_set(text_input, 0, sizeof(text_input_t));
    return 0;
}

int text_input_add_char(text_input_t *text_input, char c) {
    if (text_input->length + 1 > text_input->capacity) {
        char *new_text = (char *)kmalloc(text_input->capacity * 2);
        if (new_text == NULL) {
            console_write("Failed to allocate memory for text input\r\n");
            return -1;
        }

        memory_copy(new_text, text_input->text, text_input->length);
        kfree(text_input->text);

        text_input->text = new_text;
        text_input->capacity *= 2;
    }

    // Move the text after the cursor to the right.
    if (text_input->cursor_index < text_input->length) {
        for (uint32_t i = text_input->length; i > text_input->cursor_index; i--) {
            text_input->text[i] = text_input->text[i - 1];
        }
        text_input->length++;
    }

    text_input->text[text_input->cursor_index] = c;
    text_input->cursor_index++;

    if (text_input->cursor_index > text_input->length) {
        text_input->length = text_input->cursor_index;
    }

    // Make sure it is null terminated.
    text_input->text[text_input->length] = '\0';

    return 0;
}

int text_input_remove_char(text_input_t *text_input) {
    if (text_input->cursor_index == 0) return 0;

    char c = text_input->text[text_input->cursor_index - 1];
    uint8_t move_up = c == '\n';

    // If cursor is at the end, just remove the last character.
    if (text_input->cursor_index == text_input->length) {
        text_input->text[text_input->cursor_index - 1] = '\0';
        text_input->length--;
        text_input->cursor_index--;
        return 0;
    }

    // Otherwise shift all text from the cursor to the left.
    for (uint32_t i = text_input->cursor_index; i < text_input->length; i++) {
        text_input->text[i - 1] = text_input->text[i];
    }
    text_input->length--;

    // Make sure it is null terminated.
    text_input->text[text_input->length] = '\0';
    text_input->cursor_index--;

    return 0;
}

int text_input_move_cursor(text_input_t *text_input, int32_t dx) {
    if (dx == 0) return 0;
    if ((int32_t)text_input->cursor_index + dx < 0) return 0;
    if ((int32_t)text_input->cursor_index + dx > text_input->length) return 0;

    text_input->cursor_index += dx;

    return 0;
}

static void text_input_update(text_input_t *text_input) {
    volatile usb_hid_keyboard_report_t *keyboard_report = usb_hid_keyboard_get_report();
    if (keyboard_report == NULL) {
        return;
    }

    for (uint8_t i = 0; i < 6; i++) {
        if (keyboard_report->keypress[i] == 0) {
            text_input->key_down[i] = 0;
            continue;
        }

        if (text_input->key_down[i]) {
            continue;
        }

        uint8_t scancode = keyboard_report->keypress[i];
        text_input->key_down[i] = 1;

        uint8_t shift_down = (keyboard_report->modifiers & KEY_MOD_LSHIFT) || (keyboard_report->modifiers & KEY_MOD_RSHIFT);

        if (scancode >= KEY_A && scancode <= KEY_Z) {
            if (shift_down) {
                char c = scancode - KEY_A + 'A';
                text_input_add_char(text_input, c);
            } else {
                char c = scancode - KEY_A + 'a';
                text_input_add_char(text_input, c);
            }
        }

        if (scancode >= KEY_1 && scancode <= KEY_0 && !shift_down) {
            char c = scancode - KEY_1 + '1';
            text_input_add_char(text_input, c);
        }

        if (scancode == KEY_1 && shift_down) {
            text_input_add_char(text_input, '!');
        }

        if (scancode == KEY_2 && shift_down) {
            text_input_add_char(text_input, '@');
        }

        if (scancode == KEY_3 && shift_down) {
            text_input_add_char(text_input, '#');
        }

        if (scancode == KEY_4 && shift_down) {
            text_input_add_char(text_input, '$');
        }

        if (scancode == KEY_5 && shift_down) {
            text_input_add_char(text_input, '%');
        }

        if (scancode == KEY_6 && shift_down) {
            text_input_add_char(text_input, '^');
        }

        if (scancode == KEY_7 && shift_down) {
            text_input_add_char(text_input, '&');
        }

        if (scancode == KEY_8 && shift_down) {
            text_input_add_char(text_input, '*');
        }

        if (scancode == KEY_9 && shift_down) {
            text_input_add_char(text_input, '(');
        }

        if (scancode == KEY_0 && shift_down) {
            text_input_add_char(text_input, ')');
        }

        if (scancode == KEY_MINUS) {
            text_input_add_char(text_input, shift_down ? '_' : '-');
        }

        if (scancode == KEY_EQUAL) {
            text_input_add_char(text_input, shift_down ? '+' : '=');
        }

        if (scancode == KEY_SPACE) {
            text_input_add_char(text_input, ' ');
        }

        if (scancode == KEY_BACKSPACE) {
            text_input_remove_char(text_input);
        }

        if (scancode == KEY_SLASH) {
            text_input_add_char(text_input, shift_down ? '?' : '/');
        }

        if (scancode == KEY_DOT) {
            text_input_add_char(text_input, shift_down ? '>' : '.');
        }

        if (scancode == KEY_COMMA) {
            text_input_add_char(text_input, shift_down ? '<' : ',');
        }

        if (scancode == KEY_SEMICOLON) {
            text_input_add_char(text_input, shift_down ? ':' : ';');
        }

        if (scancode == KEY_APOSTROPHE) {
            text_input_add_char(text_input, shift_down ? '"' : '\'');
        }

        if (scancode == KEY_LEFTBRACE) {
            text_input_add_char(text_input, shift_down ? '{' : '[');
        }

        if (scancode == KEY_RIGHTBRACE) {
            text_input_add_char(text_input, shift_down ? '}' : ']');
        }

        if (scancode == KEY_BACKSLASH) {
            text_input_add_char(text_input, shift_down ? '|' : '\\');
        }

        if (scancode == KEY_GRAVE) {
            text_input_add_char(text_input, shift_down ? '~' : '`');
        }

        if (scancode == KEY_LEFT) {
            text_input_move_cursor(text_input, -1);
        }

        if (scancode == KEY_RIGHT) {
            text_input_move_cursor(text_input, 1);
        }

        if (scancode == KEY_ENTER) {
            text_input_add_char(text_input, '\n');
        }
    }
}

int text_input_draw(text_input_t *text_input) {
    // Update the state of the text input.
    text_input_update(text_input);

    // Draw the content of the text input.
    if (strlen(text_input->text) > 0) {
        font_draw_text(
            text_input->text,
            text_input->position.x,
            text_input->position.y + font_get_ascent(),
            text_input->text_color
        );
    }

    // Draw blinking cursor.
    if (get_time_ms() % 1000 < 500) {
        if (text_input->cursor_index == 0) {
            gfx_fill_rect(
                text_input->position.x,
                text_input->position.y,
                text_input->cursor_size.x,
                text_input->cursor_size.y,
                text_input->cursor_color
            );
        } else {
            int32_t cursor_x, cursor_y;
            char c = text_input->text[text_input->cursor_index - 1];
            int x_off = c == '\n' ? 0 : TEXT_INPUT_CHAR_WIDTH;
            font_calculate_cursor_pos(
                text_input->text,
                text_input->cursor_index - 1,
                text_input->position.x,
                text_input->position.y + font_get_ascent(),
                &cursor_x,
                &cursor_y
            );
            gfx_fill_rect(
                cursor_x + x_off,
                cursor_y - font_get_ascent(),
                text_input->cursor_size.x,
                text_input->cursor_size.y,
                text_input->cursor_color
            );
        }
    }

    return 0;
}
