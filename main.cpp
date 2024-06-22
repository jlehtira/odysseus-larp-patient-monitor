
#ifdef __cplusplus
    #include <cstdlib>
#else
    #include <stdlib.h>
#endif

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <stdio.h>
#include <string>
#include <cmath>

// #include <pthread.h>
#include <cassert>


#include "lightstone/lightstone.h"

//Screen dimension constants
int SCREEN_WIDTH = 1920; //640;
int SCREEN_HEIGHT = 1080; //480;

const int MAX_DATALEN = 5000;
int PLOT_WIDTH = SCREEN_WIDTH / 2;
int PLOT_HEIGHT = SCREEN_HEIGHT / 2;
int PLOT_MARGIN = 20; // pixels

int FONT_SIZE =500;
int DATA_WIDTH = 10;
int LINE_WIDTH = 5;


const float HR_MIN = 30;
const float HR_MAX = 160;

const float SCL_MIN = 0;
const float SCL_MAX = 100;


bool AUDIO = true;
SDL_AudioDeviceID AUDIOdev;
struct WAV {
    SDL_AudioSpec spec;
    Uint32 length;
    Uint8 *buffer;
};
WAV beepsound, heartsound;
int beepcooldown = 0;


// GLOBAL VARIABLES because latest fashion, er, threads
bool quit=false;
int numdev, alertphase = 0;
lightstone *lsdev[4];
lightstone_info ls[4];
int heartline[4] = { 0, 0, 0, 0 };
float heart[4][MAX_DATALEN], scl[4][MAX_DATALEN];


float clamp(float val, float minval, float maxval) {
    if (val < minval) val = minval;
    if (val > maxval) val = maxval;
    return val;
}

void plot_curve_heart(SDL_Renderer *gRenderer, float values[], int quadrant = 0) { // 0: whole screen, 1 - 4 quadrants
    int X0, Y0, W = SCREEN_WIDTH/2 - 3*PLOT_MARGIN, H = SCREEN_HEIGHT/2 - 3*PLOT_MARGIN;
    if (quadrant == 0) {
        W = SCREEN_WIDTH - 2*PLOT_MARGIN;
        H = SCREEN_HEIGHT - 2*PLOT_MARGIN;
        X0 = Y0 = PLOT_MARGIN;
    } else if (quadrant == 1) {
        X0 = Y0 = PLOT_MARGIN;
        if (numdev == 2) H = SCREEN_HEIGHT - 2*PLOT_MARGIN;
    } else if (quadrant == 2) {
        X0 = SCREEN_WIDTH/2 + 2*PLOT_MARGIN;
        Y0 = PLOT_MARGIN;
        if (numdev == 2) H = SCREEN_HEIGHT - 2*PLOT_MARGIN;
    } else if (quadrant == 3) {
        X0 = PLOT_MARGIN;
        Y0 = SCREEN_HEIGHT/2 + 2*PLOT_MARGIN;
    } else if (quadrant == 4) {
        X0 = SCREEN_WIDTH/2 + 2*PLOT_MARGIN;
        Y0 = SCREEN_HEIGHT/2 + 2*PLOT_MARGIN;
    }

    if (LINE_WIDTH == 1) {
        SDL_Point curve[W];
        float hr;
        for (int i=0; i<W/DATA_WIDTH; i++) {
           curve[i].x = X0 + W - i*DATA_WIDTH;
            hr = clamp(values[i], HR_MIN, HR_MAX);
            curve[i].y = Y0 + H - H*(hr / (HR_MAX - HR_MIN));
//        50*(i%10) + (i%3)*14 + ((i>>3)%9)*80;
        }
        SDL_SetRenderDrawColor( gRenderer, 0xCC, 0x33, 0x33, 0xFF );
        SDL_RenderDrawLines(gRenderer, curve, W/DATA_WIDTH);
    } else {
        int x0, y0, x1, y1;
        float scl;

        x0 = X0 + W;
        y0 = Y0 + H - H*(clamp(values[0], HR_MIN, HR_MAX) / (HR_MAX - HR_MIN));
        for (int i=1; i<W/DATA_WIDTH; i++) {
            scl = clamp(values[i], HR_MIN, HR_MAX);
            x1 = X0 + W - i*DATA_WIDTH;
            y1 = Y0 + H - H*(scl / (HR_MAX - HR_MIN));
            thickLineRGBA(gRenderer, x0, y0, x1, y1, LINE_WIDTH, 0xFF, 0x33, 0x33, 0xFF);
            x0 = x1; y0 = y1;
        }
    }

}


void plot_heart_alert(SDL_Renderer *gRenderer, int idev, int quadrant = 0) {
    int X0, Y0, W = SCREEN_WIDTH/2 - 3*PLOT_MARGIN, H = SCREEN_HEIGHT/2 - 3*PLOT_MARGIN;
    if (quadrant == 0) {
        W = SCREEN_WIDTH - 2*PLOT_MARGIN;
        H = SCREEN_HEIGHT - 2*PLOT_MARGIN;
        X0 = Y0 = PLOT_MARGIN;
    } else if (quadrant == 1) {
        X0 = Y0 = PLOT_MARGIN;
        if (numdev == 2) H = SCREEN_HEIGHT - 2*PLOT_MARGIN;
    } else if (quadrant == 2) {
        X0 = SCREEN_WIDTH/2 + 2*PLOT_MARGIN;
        Y0 = PLOT_MARGIN;
        if (numdev == 2) H = SCREEN_HEIGHT - 2*PLOT_MARGIN;
    } else if (quadrant == 3) {
        X0 = PLOT_MARGIN;
        Y0 = SCREEN_HEIGHT/2 + 2*PLOT_MARGIN;
    } else if (quadrant == 4) {
        X0 = SCREEN_WIDTH/2 + 2*PLOT_MARGIN;
        Y0 = SCREEN_HEIGHT/2 + 2*PLOT_MARGIN;
    }

    if (heartline[idev] > 50) {
        SDL_Rect alertRect = { X0, Y0, W, H };
        SDL_SetRenderDrawColor( gRenderer, alertphase, 0x00, 0x00, 0xFF );
        SDL_RenderFillRect(gRenderer, &alertRect);

        if (AUDIO && (beepcooldown == 0)) {
            SDL_QueueAudio(AUDIOdev, beepsound.buffer, beepsound.length);
            SDL_PauseAudioDevice(AUDIOdev, 0);
            beepcooldown = 500;
        }
    }
}

void plot_curve_scl(SDL_Renderer *gRenderer, float values[], int quadrant = 0) { // 0: whole screen, 1 - 4 quadrants
    int X0, Y0, W = SCREEN_WIDTH/2 - 3*PLOT_MARGIN, H = SCREEN_HEIGHT/2 - 3*PLOT_MARGIN;
    if (quadrant == 0) {
        W = SCREEN_WIDTH - 2*PLOT_MARGIN;
        H = SCREEN_HEIGHT - 2*PLOT_MARGIN;
        X0 = Y0 = PLOT_MARGIN;
    } else if (quadrant == 1) {
        X0 = Y0 = PLOT_MARGIN;
        if (numdev == 2) H = SCREEN_HEIGHT - 2*PLOT_MARGIN;
    } else if (quadrant == 2) {
        X0 = SCREEN_WIDTH/2 + 2*PLOT_MARGIN;
        Y0 = PLOT_MARGIN;
        if (numdev == 2) H = SCREEN_HEIGHT - 2*PLOT_MARGIN;
    } else if (quadrant == 3) {
        X0 = PLOT_MARGIN;
        Y0 = SCREEN_HEIGHT/2 + 2*PLOT_MARGIN;
    } else if (quadrant == 4) {
        X0 = SCREEN_WIDTH/2 + 2*PLOT_MARGIN;
        Y0 = SCREEN_HEIGHT/2 + 2*PLOT_MARGIN;
    }

    if (LINE_WIDTH == 1) {
        SDL_Point curve[W];
        float scl;
        for (int i=0; i<W/DATA_WIDTH; i++) {
            curve[i].x = X0 + W - i*DATA_WIDTH;
            scl = clamp(values[i], SCL_MIN, SCL_MAX);
            curve[i].y = Y0 + H - H*(scl / (SCL_MAX - SCL_MIN));
//        50*(i%10) + (i%3)*14 + ((i>>3)%9)*80;
        }
        SDL_SetRenderDrawColor( gRenderer, 0x33, 0x33, 0xCC, 0xFF );
        SDL_RenderDrawLines(gRenderer, curve, W/DATA_WIDTH);
    } else {
        int x0, y0, x1, y1;
        float scl;

        x0 = X0 + W;
        y0 = Y0 + H - H*(clamp(values[0], SCL_MIN, SCL_MAX) / (SCL_MAX - SCL_MIN));
        for (int i=1; i<W/DATA_WIDTH; i++) {
            scl = clamp(values[i], SCL_MIN, SCL_MAX);
            x1 = X0 + W - i*DATA_WIDTH;
            y1 = Y0 + H - H*(scl / (SCL_MAX - SCL_MIN));
            thickLineRGBA(gRenderer, x0, y0, x1, y1, LINE_WIDTH, 0x33, 0x33, 0xCC, 0xFF);
            x0 = x1; y0 = y1;
        }
    }
}



void curve_append(float values[], int len, float val) {
    float temp, ins = val;
    for (int i=0; i<len; i++) {
        temp = values[i];
        values[i] = ins;
        ins = temp;
    }
}


//void *read_thread(void *number){ // JL this worked with pthreads
int read_thread(void *number){ // SDL
    int i = *((int *)number);
    printf("Thread %d created!\n", i);

    while (quit == false) {
        // Read from LightStone device(s)
        ls[i] = lightstone_get_info(lsdev[i]);

        if (ls[i].hrv < 0) {
            printf("Error reading lightstone device %d!\n", i+1);
//            break;
        }
        ls[i].scl = clamp(ls[i].scl, 0.0, 10.0);
        float sclmod, hrvmod;
        ls[i].scl == 0 ? sclmod = 0 : sclmod = 10.0 * (10.0 - ls[i].scl);
        hrvmod = 60.0 / clamp(ls[i].hrv, 0.3, 5.0);
        if ((ls[i].hrv < 5.0) && (ls[i].hrv > 0.1)) { // This IF will filter out obviously bad values
            curve_append(heart[i], MAX_DATALEN, hrvmod);
            curve_append(scl[i], MAX_DATALEN, sclmod);

            if ((fabs(ls[i].hrv - 1) < 0.06) && (ls[i].scl != 0.0)) heartline[i]++;
            else heartline[i] = 0;
        }
    }
    return 0;
}

bool open_audio() {
    SDL_AudioSpec have;

    SDL_Init(SDL_INIT_AUDIO);

    SDL_LoadWAV("Bleep-SoundBible.com-1927126940.wav", &beepsound.spec, &beepsound.buffer, &beepsound.length);

    AUDIOdev = SDL_OpenAudioDevice(NULL, 0, &beepsound.spec, &have, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
    if (AUDIOdev == 0) {
        SDL_Log("Failed to open audio: %s", SDL_GetError());
        return false;
    }

//    SDL_LoadWAV("Real Heart Beat-SoundBible.com-717420031.wav", &heartsound.spec, &heartsound.buffer, &heartsound.length);

    return true;
}


// /////////////////////////////////////////////////


int main( int argc, char* args[] )
{
	SDL_Window* window = NULL;
	SDL_Surface* screenSurface = NULL;
    SDL_Renderer* gRenderer = NULL;

    int ret, i;


    // Initialize liblightstone
    lsdev[0]  = lightstone_create();
    numdev = lightstone_get_count(lsdev[0]);
    if(!numdev) {
        printf("No lightstones connected!\n");
        return 1;
    } else  printf("Found %d Lightstones\n", numdev);

    // Open lightstone devices
    ret = lightstone_open(lsdev[0], 0);
    if(ret < 0) {
        printf("Cannot open lightstone!\n");
        return 1;
    }
    for (i=1; i<numdev; i++) {
        lsdev[i] = lightstone_create();
        ret = lightstone_open(lsdev[i], i);
        if(ret < 0) {
            printf("Cannot open lightstone number %d!\n", i+1);
           return 1;
        }
    }


	//Initialize SDL
	if( SDL_Init( SDL_INIT_VIDEO ) < 0 ) {
		printf( "SDL could not initialize! SDL_Error: %s\n", SDL_GetError() );
		return -1;
	}
	//Initialize SDL_ttf
    if( TTF_Init() == -1 ) {
        printf( "SDL_ttf could not initialize! SDL_ttf Error: %s\n", TTF_GetError() );
        quit = true;
    }
    //Initialize audio
    if (AUDIO) open_audio();


    //Create window
    window = SDL_CreateWindow( "SDL Tutorial", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP );
    if( window == NULL )
    {
        printf( "Window could not be created! SDL_Error: %s\n", SDL_GetError() );
        quit = true;
    } else {
        SDL_GetWindowSize(window, &SCREEN_WIDTH, &SCREEN_HEIGHT);
        PLOT_WIDTH = SCREEN_WIDTH / 2;
        PLOT_HEIGHT = SCREEN_HEIGHT / 2;
// Renderer, not Surface (SDL2)
        gRenderer = SDL_CreateRenderer( window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC );
        if( gRenderer == NULL ) {
            printf( "Renderer could not be created! SDL Error: %s\n", SDL_GetError() );
            quit = true;
        }
        SDL_SetRenderDrawColor( gRenderer, 0xFF, 0xFF, 0xFF, 0xFF );
    }
    SDL_ShowCursor(false);


    // Open the font
    TTF_Font *gFont = TTF_OpenFont( "bin/Debug/neuropol_x_rg.ttf", FONT_SIZE );
//    TTF_Font *gFont = TTF_OpenFont( "bin/Debug/DELIRIUM_NCV.ttf", FONT_SIZE );
    if (gFont == NULL) gFont = TTF_OpenFont( "neuropol_x_rg.ttf", FONT_SIZE );
    if( gFont == NULL ) {
        printf( "Failed to load lazy font! SDL_ttf Error: %s\n", TTF_GetError() );
        quit = true;
    }


    // Create titles
    SDL_Color black = {0, 0, 0}, white = {255, 255, 255}, red = {0xCC, 0x33, 0x33}, blue = {0x33, 0x33, 0xCC};
    SDL_Surface *textSfc = TTF_RenderText_Solid (gFont, "ABC", white);
    SDL_Texture* textTex = SDL_CreateTextureFromSurface(gRenderer, textSfc);

    SDL_Surface *htitlesfc[4], *stitlesfc[4];
    SDL_Texture *htitletex[4], *stitletex[4];

    for (i=0; i<numdev; i++) {
        htitlesfc[i] = TTF_RenderText_Solid (gFont, "HEART", red);
        htitletex[i] = SDL_CreateTextureFromSurface(gRenderer, htitlesfc[i]);

        stitlesfc[i] = TTF_RenderText_Solid (gFont, "VITALITY", blue);
        stitletex[i] = SDL_CreateTextureFromSurface(gRenderer, stitlesfc[i]);
    }


// Test noise
//    float heart_data[MAX_DATALEN], scl_data[MAX_DATALEN], heart_1[MAX_DATALEN], scl_1[MAX_DATALEN];
//    for (int i=0; i<MAX_DATALEN; i++) {
//        heart_data[i] = 30 + 3*(i%10) + (i%3)*5 + ((i>>3)%9);
//        scl_data[i] = 1.0 + float(i%7)/3 + float((i>>4)%19)/7;
//    }
//    heart_data[10] = 0.0;
//    heart_data[11] = 110.0;
//    scl_data[100] = 0.0;
//    scl_data[200] = 15.0;


    // Create the threads that will read values
    int tid[4];
/* pthreads
    pthread_t threads[4];
    for (int i=0; i<numdev; i++) {
        tid[i] = i;
        printf ("Creating thread %d\n",tid[i]);
        int res = pthread_create(&threads[tid[i]], NULL, read_thread, &tid[i]);
        assert (!res);
    }*/
/* SDL Threads */
    SDL_Thread *threads[4];
    for (int i=0; i<numdev; i++) {
        tid[i] = i;
        threads[i] = SDL_CreateThread(read_thread, "read_thread", &tid[i]);
    }


    SDL_Event e;
    int frame = 0, disp;
    while (!quit) {


//        SDL_FillRect( screenSurface, &dstRect, SDL_MapRGB( screenSurface->format, 0x00, 0xAA, 0x00 ) ); // Fill the surface
//        SDL_BlitSurface(textSfc, NULL, screenSurface, &dstRect);
//        SDL_UpdateWindowSurface( window );

        SDL_SetRenderDrawColor( gRenderer, 0x00, 0x00, 0x00, 0xFF );
        SDL_RenderClear(gRenderer);

        for (i=0; i<numdev; i++) {
//        for (i=0; i<1; i++) {
            numdev > 1? disp = i+1 : disp = 0;

            plot_heart_alert(gRenderer, i, disp);

            plot_curve_heart(gRenderer, heart[i], disp);
            plot_curve_scl(gRenderer, scl[i], disp);

            int X0, Y0, W = SCREEN_WIDTH/2 - 3*PLOT_MARGIN, H = SCREEN_HEIGHT/2 - 3*PLOT_MARGIN;
            if (i == 0) {
                X0 = Y0 = PLOT_MARGIN;
            } else if (i == 1) {
                X0 = SCREEN_WIDTH/2 + 2*PLOT_MARGIN;
                Y0 = PLOT_MARGIN;
            } else if (i == 2) {
                X0 = PLOT_MARGIN;
                Y0 = SCREEN_HEIGHT/2 + 2*PLOT_MARGIN;
            } else if (i == 3) {
                X0 = SCREEN_WIDTH/2 + 2*PLOT_MARGIN;
                Y0 = SCREEN_HEIGHT/2 + 2*PLOT_MARGIN;
            }
//            char htitle[80], stitle[80];
//            sprintf(htitle, "HEART %.1f", heart[i][0]);
//            sprintf(stitle, "VITALITY %.1f", scl[i][0]);

//            htitlesfc[i] = TTF_RenderText_Solid (gFont, htitle, red);
//            htitletex[i] = SDL_CreateTextureFromSurface(gRenderer, htitlesfc[i]);

//            stitlesfc[i] = TTF_RenderText_Solid (gFont, stitle, blue);
//            stitletex[i] = SDL_CreateTextureFromSurface(gRenderer, stitlesfc[i]);

            SDL_Rect dstRect = { X0+10, Y0+10, W/3, H/3 };
            SDL_RenderCopy(gRenderer, htitletex[i], NULL, &dstRect);
            dstRect = { X0+10, Y0+10+H/3, W/3, H/3 };
            SDL_RenderCopy(gRenderer, stitletex[i], NULL, &dstRect);

//            SDL_DestroyTexture(htitletex[i]);
//            SDL_DestroyTexture(stitletex[i]);
//            SDL_FreeSurface(htitlesfc[i]);
//            SDL_FreeSurface(stitlesfc[i]);
        }

        SDL_RenderPresent(gRenderer);


        while( SDL_PollEvent( &e ) != 0 ) //Handle events on queue
        {
            //User requests quit
            if( e.type == SDL_QUIT ) {
                quit = true;
            } else if (e.type == SDL_KEYUP) {
                switch (e.key.keysym.sym) {
                case SDLK_ESCAPE:
                    quit = true;
                    break;
                case SDLK_KP_PLUS:
                case SDLK_PLUS:
                    if (LINE_WIDTH < 20) LINE_WIDTH++;
                    break;
                case SDLK_KP_MINUS:
                case SDLK_MINUS:
                case SDLK_0:
                    if (LINE_WIDTH > 1) LINE_WIDTH--;
                    break;
                case SDLK_4:
                    if (DATA_WIDTH > 1) DATA_WIDTH--;
                    break;
                case SDLK_5:
                    if (DATA_WIDTH < 50) DATA_WIDTH++;
                    break;

                case SDLK_a:
                case SDLK_b:
                case SDLK_6:
                    if (AUDIO == true) AUDIO = false;
                    else AUDIO = true;
                    break;

                case SDLK_1:
                    if (numdev >= 2) std::swap (lsdev[0], lsdev[1]);
                    break;
                case SDLK_2:
                    if (numdev >= 3) std::swap (lsdev[1], lsdev[2]);
                    break;
                case SDLK_3:
                    if (numdev == 4) std::swap (lsdev[2], lsdev[3]);
                    break;
                }
            }
        }

        SDL_Delay(10);

        // Read from LightStone device(s)

        // TOTALLY MOVED TO DEDICATED THREADS !!!!!

// Test noise
//        curve_append(heart_data, MAX_DATALEN, 60 + 5*(frame%10) + (frame%3)*8 + ((frame>>3)%9)*2);
//        curve_append(scl_data, MAX_DATALEN, 1.0 + float(frame%7)/3 + float((frame>>4)%19)/7);

        frame++;
        if (++alertphase >160) alertphase = 0;
        if (beepcooldown > 0) beepcooldown--;
    }



    if (gFont) TTF_CloseFont (gFont);
	SDL_DestroyWindow( window );
	SDL_Quit();

//    lightstone_delete(lslib);
    for (i=0; i<numdev; i++) lightstone_delete(lsdev[i]);

	return 0;
}
