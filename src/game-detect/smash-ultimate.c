#include <stdlib.h>
#include <obs-module.h>
#include <plugin-support.h>
#include "ocr.h"
#include "string-utils.h"
#include "smash-ultimate.h"

#define NUM_SMASH_CHARACTERS 2
#define LEVENSHTIEN_THRESHOLD 4

static char *character_list[] = {
	"MARIO",
	"DONKEY KONG",
	"LINK",
	"SAMUS",
	"DARK SAMUS",
	"YOSHI",
	"KIRBY",
	"FOX",
	"PIKACHU",
	"LUIGI",
	"NESS",
	"CAPTAIN FALCON",
	"JIGGLYPUFF",
	"PEACH",
	"DAISY",
	"BOWSER",
	"ICE CLIMBERS",
	"SHEIK",
	"ZELDA",
	"DR. MARIO",
	"PICHU",
	"FALCO",
	"MARTH",
	"LUCINA",
	"YOUNG LINK",
	"GANONDORF",
	"MEWTWO",
	"ROY",
	"CHROM",
	"MR. GAME & WATCH",
	"META KNIGHT",
	"PIT",
	"DARK PIT",
	"ZERO SUIT SAMUS",
	"WARIO",
	"SNAKE",
	"IKE",
	"POKEMON TRAINER",
	"DIDDY KONG",
	"LUCAS",
	"SONIC",
	"KING DEDEDE",
	"OLIMAR",
	"LUCARIO",
	"R.O.B.",
	"TOON LINK",
	"WOLF",
	"VILLAGER",
	"MEGA MAN",
	"WII FIT TRAINER",
	"ROSALINA & LUMA",
	"LITTLE MAC",
	"GRENINJA",
	"PALUTENA",
	"PAC-MAN",
	"ROBIN",
	"SHULK",
	"BOWSER JR.",
	"DUCK HUNT",
	"RYU",
	"KEN",
	"CLOUD",
	"CORRIN",
	"BAYONETTA",
	"INKLING",
	"RIDLEY",
	"SIMON",
	"RICHTER",
	"KING K. ROOL",
	"ISABELLE",
	"INCINEROAR",
	"PIRANHA PLANT",
	"JOKER",
	"HERO",
	"BANJO & KAZOOIE",
	"TERRY",
	"BYLETH",
	"MIN MIN",
	"STEVE",
	"SEPHIROTH",
	"PYRA/MYTHRA",
	"KAZUYA",
	"SORA",
	// Mii's cant be recognized since they get separate names
};

static struct expected_pixel_area loadin_screen_detector[] = {
	// GREY AREAS
	{
		// top left corner
		.rgba = {0x36, 0x43, 0x48, 0xFF},
		.pixel_threshold = 10,
		.startx = 0,
		.endx = 16,
		.starty = 0,
		.endy = 1,
	},
	{
		// top left center
		.rgba = {0x36, 0x43, 0x48, 0xFF},
		.pixel_threshold = 10,
		.startx = 480,
		.endx = 496,
		.starty = 0,
		.endy = 1,
	},
	{
		// top right center
		.rgba = {0x36, 0x43, 0x48, 0xFF},
		.pixel_threshold = 10,
		.startx = 1440,
		.endx = 1456,
		.starty = 0,
		.endy = 1,
	},
	{
		// center,
		.rgba = {0x36, 0x43, 0x48, 0xFF},
		.pixel_threshold = 10,
		.startx = 968,
		.endx = 984,
		.starty = 0,
		.endy = 1,
	},
	// BLACK AREAS
	{
		// top left corner, beginning of name background
		.rgba = {0x0f, 0x10, 0x18, 0xFF},
		.pixel_threshold = 10,
		.startx = 0,
		.endx = 1,
		.starty = 34,
		.endy = 42,
	},
	{
		// center of name background
		.rgba = {0x0f, 0x10, 0x18, 0xFF},
		.pixel_threshold = 10,
		.startx = 960,
		.endx = 961,
		.starty = 34,
		.endy = 42,
	},
};

static char *get_character_name(char *text)
{
	if (text == NULL) {
		return NULL;
	}

	for (uint32_t i = 0; i < sizeof(character_list) / sizeof(character_list[0]); i++) {
		if (str_approximate_match(text, character_list[i], LEVENSHTIEN_THRESHOLD)) {
			return character_list[i];
		}
	}
	return NULL;
}

static void get_character_name_image(struct frame_data *in_frame, struct frame_data *out_frame,
				     uint32_t startx, uint32_t endx, uint32_t starty, uint32_t endy)
{
	for (uint32_t y = starty; y < endy; y++) {
		for (uint32_t x = startx; x < endx; x++) {
			uint32_t in_index = (y * in_frame->width + x) * 4;
			uint32_t out_index = ((y - starty) * out_frame->width + x - startx) * 4;

			uint8_t r = in_frame->rgba_data[in_index + 0];
			uint8_t g = in_frame->rgba_data[in_index + 1];
			uint8_t b = in_frame->rgba_data[in_index + 2];

			if (r >= 200 && g >= 200 && b >= 200) {
				// close enough to white becomes black
				out_frame->rgba_data[out_index + 0] = 0;   // R
				out_frame->rgba_data[out_index + 1] = 0;   // G
				out_frame->rgba_data[out_index + 2] = 0;   // B
				out_frame->rgba_data[out_index + 3] = 255; // A
			} else {
				// everything else becomes white
				out_frame->rgba_data[out_index + 0] = 255; // R
				out_frame->rgba_data[out_index + 1] = 255; // G
				out_frame->rgba_data[out_index + 2] = 255; // B
				out_frame->rgba_data[out_index + 3] = 255; // A
			}
		}
	}
}

static void get_character_name_boxes(struct frame_data *in_frame, struct frame_data *out_frames)
{
	frame_data_init(&out_frames[0], in_frame->width * 3 / 8, in_frame->height / 8);
	get_character_name_image(in_frame, &out_frames[0],
				 in_frame->width * 1 / 16, // startx
				 in_frame->width * 7 / 16, // endx
				 0,                        // starty
				 in_frame->height * 1 / 8  // endy
	);

	frame_data_init(&out_frames[1], in_frame->width * 3 / 8, in_frame->height / 8);
	get_character_name_image(in_frame, &out_frames[1],
				 in_frame->width * 9 / 16,  // startx
				 in_frame->width * 15 / 16, // endx
				 0,                         // starty
				 in_frame->height * 1 / 8   // endy
	);

	// for debugging
	obs_log(LOG_INFO, "Writing PNG files");
	img_write_png(&out_frames[0], "/Users/Tom/Desktop/character0.png");
	img_write_png(&out_frames[1], "/Users/Tom/Desktop/character1.png");
	img_write_png(in_frame, "/Users/Tom/Desktop/both.png");
}

void ssbu_detect(struct frame_data *frame)
{
	struct frame_data name_boxes[NUM_SMASH_CHARACTERS] = {0};

	obs_log(LOG_INFO, "--------------------------------------------------");
	obs_log(LOG_INFO, "LOADIN SCREEN DETECTED");
	get_character_name_boxes(frame, name_boxes);

	for (int i = 0; i < NUM_SMASH_CHARACTERS; i++) {
		char *text = ocr_analyze_for_text(&name_boxes[i]);
		frame_data_destroy(&name_boxes[i]);

		char *character_name = get_character_name(text);
		obs_log(LOG_INFO, "Original: %s, Result: %s", text, character_name);
		free(text);
	}
}

bool ssbu_detect_loadin_screen(struct frame_data *frame)
{
	float matches = 0.0f;
	uint32_t num_areas = sizeof(loadin_screen_detector) / sizeof(struct expected_pixel_area);

	for (uint32_t i = 0; i < num_areas; i++) {
		matches += img_check_expected_pixels(frame, &loadin_screen_detector[i]);
	}

	return matches / (float)num_areas >= 1.0f;
}
