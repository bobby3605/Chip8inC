#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <sys/time.h>
#include <stdbool.h>
#include <math.h>

const int SCREEN_WIDTH = 64*8;
const int SCREEN_HEIGHT = 32*8;

SDL_Event e;

SDL_Window* window = NULL;
SDL_Surface* sdlSurface = NULL;
SDL_Renderer* renderer = NULL;
SDL_Texture* texture = NULL;

struct STACK
{
	uint16_t size[16];
	unsigned int top;
};

struct CHIP8
{
	// 4k size, arrays start at 0
	unsigned char RAM[4096];

	// Program counter
	unsigned int PC;

	// Display data
	unsigned int Display[64 * 32];

	// Initialize stack
	struct STACK stack;

	// Registers
	unsigned char V[16];

	unsigned int I;

	// Timers
	char delay_timer;

	char sound_timer;

	bool keyboard[16];
};

void emulate(struct CHIP8 chip, int size);

int main(int argc, char **argv)
{

	// SDL Stuff

	if(SDL_Init(SDL_INIT_EVERYTHING) < 0){
		printf("SDL Failed to initialize");
		exit(1);
	} else {
		window = SDL_CreateWindow("Chip8 Emulator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH , SCREEN_HEIGHT, 0);
		if (window == NULL){
			printf("Window creation failed");
			exit(1);
		} else {
			renderer = SDL_CreateRenderer(window, -1, 0);
		}
	}

	// Try to read file
	FILE *f = fopen(argv[1], "rb");
	if (f == NULL)
	{
		printf("Couldn't open file");
		exit(1);
	}
	// Get file size
	fseek(f, 0L, SEEK_END);
	int fsize = ftell(f);
	fseek(f, 0L, SEEK_SET);


	// Initialzize cpu
	struct CHIP8 chip8;
	for(unsigned char x = 0x00; x < 0x10; x++){
		chip8.keyboard[x] = false;
	}
	// Read file into ram
	fread(chip8.RAM + 512, 1, fsize, f);
	fclose(f);

	// Initialize font
	unsigned char font[80] = { 0xF0, 0x90, 0x90, 0x90, 0xF0, 0x20, 0x60, 0x20, 0x20, 0x70, 0xF0, 0x10, 0xF0, 0x80, 0xF0, 0xF0, 0x10, 0xF0, 0x10, 0xF0, 0x90, 0x90, 0xF0, 0x10, 0x10, 0xF0, 0x80, 0xF0, 0x10, 0xF0, 0xF0, 0x80, 0xF0, 0x90, 0xF0, 0xF0, 0x10, 0x20, 0x40, 0x40, 0xF0, 0x90, 0xF0, 0x90, 0xF0, 0xF0, 0x90, 0xF0, 0x10, 0xF0, 0xF0, 0x90, 0xF0, 0x90, 0x90, 0xE0, 0x90, 0xE0, 0x90, 0xE0, 0xF0, 0x80, 0x80, 0x80, 0xF0, 0xE0, 0x90, 0x90, 0x90, 0xE0, 0xF0, 0x80, 0xF0, 0x80, 0xF0, 0xF0, 0x80, 0xF0, 0x80, 0x80 };
	for (int i = 0; i<80; i++){
		chip8.RAM[i] = font[i];
	}

	// initialize emulation
	emulate(chip8, fsize);

}

unsigned char keymap(SDL_Event e){
	switch(e.key.keysym.sym){
		case SDLK_1:
			return 0x00;
		case SDLK_2:
			return 0x01;
		case SDLK_3:
			return 0x02;
		case SDLK_4:
			return 0x03;
		case SDLK_q:
			return 0x04;
		case SDLK_w:
			return 0x05;
		case SDLK_e:
			return 0x06;
		case SDLK_r:
			return 0x07;
		case SDLK_a:
			return 0x08;
		case SDLK_s:
			return 0x09;
		case SDLK_d:
			return 0x0a;
		case SDLK_f:
			return 0x0b;
		case SDLK_z:
			return 0x0c;
		case SDLK_x:
			return 0x0d;
		case SDLK_c:
			return 0x0e;
		case SDLK_v:
			return 0x0f;
		default:
			return 0xff;
	}
}

const int AMPLITUDE = 28000;
const int SAMPLE_RATE = 44100;

long long timeInMS(void) {
	struct timeval tv;

	gettimeofday(&tv,NULL);
	return (((long long)tv.tv_sec)*1000)+(tv.tv_usec/1000);
}

void emulate(struct CHIP8 chip, int size) {

	// Audio taken from https://gist.github.com/superzazu/c0f923181a9cdb82c6d84dcf8b61c8c5

  SDL_AudioDeviceID audio_device;

  // opening an audio device:
  SDL_AudioSpec audio_spec;
  SDL_zero(audio_spec);
  audio_spec.freq = 44100;
  audio_spec.format = AUDIO_S16SYS;
  audio_spec.channels = 1;
  audio_spec.samples = 1024;
  audio_spec.callback = NULL;

  audio_device = SDL_OpenAudioDevice(NULL, 0, &audio_spec, NULL, 0);
  // pushing 3 seconds of samples to the audio buffer:
  float x1 = 0;
  for (int i = 0; i < audio_spec.freq * 3; i++) {
		x1 += .1f;

  // SDL_QueueAudio expects a signed 16-bit value
  // note: "5000" here is just gain so that we will hear something
		int16_t sample = sin(x1 * 4) * 5000;

		const int sample_size = sizeof(int16_t) * 1;
		SDL_QueueAudio(audio_device, &sample, sample_size);
  }

	long long curr_time, prev_time = 0;

	int x;
	int y;
	int n;

	memset(chip.Display, 255, 64*32*sizeof(int));

	chip.stack.top = 0;

	chip.PC = 512;

	while (chip.PC <= (size + 0x200)) {

			curr_time = timeInMS();
			while(((float)(curr_time - prev_time))/100.0 < (0.01666666)){
			curr_time = timeInMS();
		}

			prev_time = timeInMS();
	if(chip.delay_timer > 0){
		chip.delay_timer--;
	}
	if(chip.sound_timer > 0){
		SDL_PauseAudioDevice(audio_device, 0);
		chip.sound_timer--;
	}
	if(chip.sound_timer <= 0){
		 SDL_PauseAudioDevice(audio_device, 1);
	}

		SDL_PollEvent(&e);
		if(e.type == SDL_QUIT){
			SDL_Quit();
			exit(0);
		}
		else if (e.type == SDL_KEYDOWN){
			if(keymap(e) != 0xff){
				chip.keyboard[keymap(e)] = true;
			}
		} else if (e.type == SDL_KEYUP){
			if(keymap(e) != 0xff){
			chip.keyboard[keymap(e)] = false;
			}
		}
		sdlSurface = SDL_CreateRGBSurfaceFrom(chip.Display, 64, 32, 32, 64 * 4, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
		texture = SDL_CreateTextureFromSurface(renderer, sdlSurface);
		SDL_RenderClear(renderer);
		SDL_RenderCopy(renderer, texture, NULL, NULL);
		SDL_RenderPresent(renderer);


		// set opcode
		unsigned int opcode = (chip.RAM[chip.PC] << 8) | (chip.RAM[chip.PC + 1]);

		unsigned int head = (opcode & 0xf000);
		unsigned int tail = (opcode & 0x0fff);

		switch (head) {
		case 0x0000:

			// Clear Display
			if (opcode == 0x00e0) {
				for (int x = 0; x < (64*32); x++) {
					chip.Display[x] = 0;
				}
			} else if (opcode == 0x00ee) {
				chip.PC = chip.stack.size[chip.stack.top];
				chip.stack.top--;
			} else {
				printf("Unsupported: %04x", opcode);
			} break;
		case 0x1000: chip.PC = tail; chip.PC -= 2; break;
		case 0x2000:
			chip.stack.top += 1;
			chip.stack.size[chip.stack.top] = chip.PC;
			chip.PC = tail;
			chip.PC -=2;
			break;
		case 0x3000:
			if (chip.V[(opcode & 0x0f00) >> 8] == (opcode & 0x00ff))
			{
				chip.PC += 2;
			} break;
		case 0x4000:
			if (chip.V[(opcode & 0x0f00) >> 8] != (opcode & 0x00ff))
			{
				chip.PC += 2;
			} break;
		case 0x5000:
			if (chip.V[(opcode & 0x0f00) >> 8] == chip.V[(opcode & 0x00f0) >> 4])
			{
				chip.PC += 2;
			} break;
		case 0x6000: chip.V[(opcode & 0x0f00) >> 8] = (opcode & 0x0ff); break;
		case 0x7000: chip.V[(opcode & 0x0f00) >> 8] = (chip.V[(opcode & 0x0f00) >> 8] + (opcode & 0x00ff)); break;
		case 0x8000:
			if ((opcode & 0x000f) == 0x0000) {
				chip.V[(opcode & 0x0f00) >> 8] = chip.V[(opcode & 0x00f0) >> 4];
			} else if ((opcode & 0x000f) == 0x0001) {
				chip.V[(opcode & 0x0f00) >> 8] = (chip.V[(opcode & 0x0f00) >> 8] | chip.V[(opcode & 0x00f0) >> 4]);
			} else if ((opcode & 0x000f) == 0x0002) {
				chip.V[(opcode & 0x0f00) >> 8] = (chip.V[(opcode & 0x0f00) >> 8] & chip.V[(opcode & 0x00f0) >> 4]);
			} else if ((opcode & 0x000f) == 0x0003) {
				chip.V[(opcode & 0x0f00) >> 8] = (chip.V[(opcode & 0x0f00) >> 8] ^ chip.V[(opcode & 0x00f0) >> 4]);
			} else if ((opcode & 0x000f) == 0x0004) {
				if ((chip.V[(opcode & 0x0f00) >> 8] += chip.V[(opcode & 0x00f0) >> 4]) > 255) {
					chip.V[15] = 1;
				}
			} else if ((opcode & 0x000f) == 0x0005) {
				if (chip.V[(opcode & 0x0f00) >> 8] > chip.V[(opcode & 0x00f0) >> 4]) {
					chip.V[15] = 1;
				} else {
					chip.V[15] = 0;
				}
				chip.V[(opcode & 0x0f00) >> 8] = (chip.V[(opcode & 0x0f00) >> 12] - chip.V[(opcode & 0x00f0) >> 4]);
			} else if ((opcode & 0x000f) == 0x0006) {
				chip.V[15] = ((chip.V[opcode & 0x000f]) & 0x1);
				chip.V[(opcode & 0x0f00) >> 8] = chip.V[(opcode & 0x0f00) >> 8] >> 1;
			} else if ((opcode & 0x000f) == 0x0007) {
				if (chip.V[(opcode & 0x00f0) >> 4] > chip.V[(opcode & 0x0f00) >> 8]) {
					chip.V[15] = 1;
				} else {
					chip.V[15] = 0;
				}
				chip.V[(opcode & 0x00f0) >> 4] = (chip.V[(opcode & 0x00f0) >> 4] - chip.V[(opcode & 0x0f00) >> 8]);
			} else if ((opcode & 0x000f) == 0x000e) {
				chip.V[15] = ((chip.V[(opcode & 0xf000) >> 16]) & 0x1);
				chip.V[(opcode & 0x0f00) >> 8] = chip.V[(opcode & 0x0f00) >> 8] << 1;
			} else {
				printf("Unsupported: %04x", opcode);
			} break;
		case 0x9000:
			if (chip.V[(opcode & 0x0f00) >> 8] != chip.V[(opcode & 0x00f0) >> 4])
			{
				chip.PC += 2;
			} break;
		case 0xa000: chip.I = (opcode & 0x0fff); break;
		case 0xb000: chip.PC = (chip.V[0] + (opcode & 0x0fff)); break;
		case 0xc000: chip.V[(opcode & 0x0f00) >> 8] = (rand() % 255) & (opcode & 0x00ff); break;
		case 0xd000:
			x = chip.V[(opcode & 0x0F00) >> 8];
			y = chip.V[(opcode & 0x00F0) >> 4];
			n = (opcode & 0x000F);

			chip.V[15] = 0;

			for (int i = 0; i < n; i++)
			{
				unsigned char mem = chip.RAM[chip.I + i];
					
				for (int j = 0; j < 8; j++){
					char pixel = ((mem >> (7-j)) & 0x01);
					int index = x + j + (y + i) * 64;

					if (index > 2047) continue;

					if (pixel == 1 && chip.Display[index] != 0) chip.V[15] = 1;
					chip.Display[index] = (chip.Display[index] != 0 && pixel == 0) || (chip.Display[index] == 0 && pixel == 1) ? 0xffffffff : 0;
				}
			}

			break;
		case 0xe000:
			if ((opcode & 0x00ff) == 0x009E) {
				x = (opcode & 0xF00) >> 8;
					if(chip.keyboard[chip.V[x]]){
						chip.PC += 2;
				}
			} else if ((opcode & 0x00ff) == 0x00A1) {
					x = (opcode & 0xF00) >> 8;
						if(!chip.keyboard[chip.V[x]]){
							chip.PC += 2;
					}
			} else {
				printf("Unsupported: %04x", opcode);
			} break;
		case 0xf000:
			if ((opcode & 0x000f) == 0x0007) {
				chip.V[(opcode & 0x0f00) >> 8] = chip.delay_timer;
			} else if ((opcode & 0x000f) == 0x000A) {
					x = (opcode & 0xF00) >> 8;
					while(1==1){
					SDL_PollEvent(&e);
					if(e.type == SDL_KEYDOWN){
						chip.V[x] = keymap(e);
						break;
					} else if(e.type == SDL_QUIT){
						SDL_Quit();
						exit(0);
					}
				}
			}else if ((opcode & 0x00ff) == 0x0015) {
				chip.delay_timer = chip.V[(opcode & 0x0f00) >> 8];
			}else if ((opcode & 0x00ff) == 0x0018) {
				chip.sound_timer = chip.V[(opcode & 0x0f00) >> 8];
			}else if ((opcode & 0x00ff) == 0x001e) {
				chip.I += chip.V[(opcode & 0x0f00) >> 8];
			}else if ((opcode & 0x00ff) == 0x0029) {
				// Idk how to do this
				chip.I = ((chip.V[(opcode & 0x0F00) >> 8]) * 5);
			}else if ((opcode & 0x00ff) == 0x0033) {
				// Might be broken
				int tx = (opcode & 0x0f00) >> 8;
				chip.RAM[chip.I] = (char)(chip.V[tx] / 100);
				chip.RAM[(int)chip.I + 1] = ((char)(chip.V[tx] % 100) / 10);
				chip.RAM[(int)chip.I + 2] = ((char)chip.V[tx] % 10);

			}else if ((opcode & 0x00ff) == 0x0055) {
				for (int i = 0; i <= ((opcode & 0x0f00)>>8); i++) {
					chip.RAM[chip.I + i] = chip.V[i];
				}
			}else if ((opcode & 0x00ff) == 0x0065) {
				for (int i = 0; i <= (opcode & 0x0f00) >> 8; i++) {
					chip.V[i] = chip.RAM[chip.I + i];
				}
			} else {
				return;
				printf("Unsupported: %04x", opcode);
			} break;
		default:
			printf("Unsupported: %04x", opcode);
		}

		chip.PC += 2;
		


	}
	printf("out of loop \n");

}
