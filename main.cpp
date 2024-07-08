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
#include <vector>
#include <deque>

#include <cassert>


#include "lightstone/lightstone.h"
#include "iirfilter.h"

//Screen dimension constants
int SCREEN_WIDTH = 1920; //640;
int SCREEN_HEIGHT = 1080; //480;

const int MAX_DATALEN = 5000;  // Odysseus 1: was 5000
int PLOT_MARGIN = 20; // pixels

int FONT_SIZE =500;
int DATA_WIDTH = 4;    // How many milliseconds per pixel
int LINE_WIDTH = 5;
int CURVE_MODE = 0;     // How to plot

const float HR_MIN = -140;        // Odysseus 1: was 0 - 160
const float HR_MAX = 420;       // Testing up to 460

const float SCL_MIN = 0;        // Odysseys 1; was 0 - 100
const float SCL_MAX = 120;

const char *RED_CURVENAME = "HEART";
const char *BLUE_CURVENAME = "SkC";

bool PLOT_BLUE = false;
bool FILTER_BAD = true;
bool PLOT_DIA_SYS = true;
bool PLOT_HR = true;

bool AUDIO = true;
SDL_AudioDeviceID AUDIOdev;
struct WAV {
    SDL_AudioSpec spec;
    Uint32 length;
    Uint8 *buffer;
};
WAV beepsound, heartsound;
int beepcooldown = 0;

SDL_Color black = {0, 0, 0, 0xFF}, white = {255, 255, 255, 0xFF}, red = {0xCC, 0x33, 0x33, 0xFF}, blue = {0x33, 0x66, 0xCC, 0xFF}; // Blue was 0x33, 0x33, 0xCC
SDL_Color green = {0, 0xCC, 0x66, 0xFF};

// This for IIR filtering of the signal, see iirfilter.h
std::vector<SOSState> states(sos.size());


// GLOBAL VARIABLES because latest fashion, er, threads
bool quit=false;
int numdev, alertphase = 0;
lightstone *lsdev[4];
lightstone_info ls[4];
float heart[4][MAX_DATALEN], scl[4][MAX_DATALEN];
Uint64 readtick[4][MAX_DATALEN];        // When was this device last updated
int curve_index[4] = { 0, 0, 0, 0 };    // Where is the read/write pointer
SDL_Rect screen;


struct patient_data {
    static const int heart = 1;
    static const int scl = 2;
    static const int pulse = 10;
    static const int dpulse = 11;

    float heartArr[MAX_DATALEN];
    float sclArr[MAX_DATALEN];
    Uint64 readtickArr[MAX_DATALEN];
    float pulseArr[MAX_DATALEN];    // filtered to show pulse
    float dpulseArr[MAX_DATALEN];   // 1st derivative of that

    int index;
    float heartConnected = 0.0, sclConnected = 0.0;

    std::deque<Uint64> pulseTicks, hrQueue;
    float heartRate;

    inline int get_index(int offset) {
        int result = index - offset;
        while (result < 0) result += MAX_DATALEN;
        return result % MAX_DATALEN;
    }

    Uint64 &readtick(int offset) {    return readtickArr[get_index(offset)];   }

    float &value(int var, int offset) {
        if (var == heart) return heartArr[get_index(offset)];
        if (var == scl) return sclArr[get_index(offset)];
        if (var == pulse) return pulseArr[get_index(offset)];
        if (var == dpulse) return dpulseArr[get_index(offset)];
        else return heartArr[get_index(offset)];        // Maybe sensible as default?!?
    }

    float max(int var) {
        if (var == heart) return HR_MAX;
        else if (var == scl) return SCL_MAX;
        else return 100.0;
    }
    float min(int var) {
        if (var == heart) return HR_MIN;
        else if (var == scl) return SCL_MIN;
        else return -100.0;
    }

    int connStatus() { return ((heartConnected > 0.5) ? 2 : 0) + ((sclConnected > 0.5) ? 1 : 0); }    // 3: both connected, 2: heart connected, 1: scl connected, 0: both disconnected
};

patient_data pdata[4];


float clamp(float val, float minval, float maxval) {
    if (val < minval) val = minval;
    if (val > maxval) val = maxval;
    return val;
}

// Append a value to a buffer, treating it as a circular buffer
template<typename valuetype>
int curve_write(valuetype values[], int &index, int len, valuetype val) {
    while (index >= len) index -= len;
    while (index < 0) index += len;
    values[index] = val;
    return index;
}
// Read a value from a buffer, treating it as a circular buffer
template<typename valuetype>
valuetype curve_read(valuetype values[], int index, int len, int offset) {
    int read_index = index - offset;
    while (read_index < 0) read_index += len;
    return values[read_index];
}


SDL_Rect getScreenQuadrant(SDL_Rect sRect, int quadrant = 0) {
    SDL_Rect result;

    if (quadrant == 0) {
        result.w = SCREEN_WIDTH - 2*PLOT_MARGIN;
        result.h = SCREEN_HEIGHT - 2*PLOT_MARGIN;
        result.x = result.y = PLOT_MARGIN;
    } else if (quadrant == 1) {
        result.x = result.y = PLOT_MARGIN;
        result.w = SCREEN_WIDTH/2 - 3*PLOT_MARGIN;
        if (numdev == 2) result.h = SCREEN_HEIGHT - 2*PLOT_MARGIN;
        else result.h = SCREEN_HEIGHT/2 - 3*PLOT_MARGIN;
    } else if (quadrant == 2) {
        result.x = SCREEN_WIDTH/2 + 2*PLOT_MARGIN;
        result.y = PLOT_MARGIN;
        result.w = SCREEN_WIDTH/2 - 3*PLOT_MARGIN;
        if (numdev == 2) result.h = SCREEN_HEIGHT - 2*PLOT_MARGIN;
        else result.h = SCREEN_HEIGHT/2 - 3*PLOT_MARGIN;
    } else if (quadrant == 3) {
        result.x = PLOT_MARGIN;
        result.y = SCREEN_HEIGHT/2 + 2*PLOT_MARGIN;
        result.w = SCREEN_WIDTH/2 - 3*PLOT_MARGIN;
        result.h = SCREEN_HEIGHT/2 - 3*PLOT_MARGIN;
    } else if (quadrant == 4) {
        result.x = SCREEN_WIDTH/2 + 2*PLOT_MARGIN;
        result.y = SCREEN_HEIGHT/2 + 2*PLOT_MARGIN;
        result.w = SCREEN_WIDTH/2 - 3*PLOT_MARGIN;
        result.h = SCREEN_HEIGHT/2 - 3*PLOT_MARGIN;
    }
    return result;
}

SDL_Rect getStripe(SDL_Rect sRect, int beginning, int end, int total)
{
    int stripeH = sRect.h / total;
    SDL_Rect result = { sRect.x, sRect.y + beginning*stripeH, sRect.w, stripeH * (end - beginning) };
    return result;
}


//void plot_curve_heart(SDL_Renderer *gRenderer, float values[], Uint64 readtick[], int curve_index, int quadrant = 0) { // 0: whole screen, 1 - 4 quadrants
//void plot_curve_heart(SDL_Renderer *gRenderer, patient_data &data, int quadrant = 0) { // 0: whole screen, 1 - 4 quadrants
void plot_curve(SDL_Renderer *gRenderer, patient_data &data, int var, SDL_Color color, int quadrant = 0) { // 0: whole screen, 1 - 4 quadrants
    SDL_Rect tRect = getScreenQuadrant(screen, quadrant);
    int W = tRect.w, H = tRect.h, X0 = tRect.x, Y0 = tRect.y;

    Uint64 drawtick = SDL_GetTicks64();
    if (LINE_WIDTH == 1) {      // OUTDATED. TODO: remove or update to use curve_read as below //
        SDL_Point curve[W];
        float hr;
        for (int i=0; i<W/DATA_WIDTH; i++) {
           curve[i].x = X0 + W - (drawtick - data.value(var, i))*DATA_WIDTH;
            hr = clamp(data.value(var, i), data.min(var), data.max(var));
            curve[i].y = Y0 + H - H*(hr / (data.max(var) - data.min(var)));
        }
        SDL_SetRenderDrawColor( gRenderer, color.r, color.g, color.b, color.a );
        SDL_RenderDrawLines(gRenderer, curve, W/DATA_WIDTH);
    } else {
        int x0, y0, x1, y1;
        float value, vspread = data.max(var) - data.min(var);

        x0 = X0 + W - (drawtick - data.readtick(0)) / DATA_WIDTH;
        y0 = Y0 + H - H*(clamp(data.value(var, 0),data.min(var),data.max(var)) - data.min(var)) / vspread;

        for (int i=1; i<MAX_DATALEN-1; i++) {
            value = clamp(data.value(var, i), data.min(var), data.max(var));
            x1 = X0 + W - (drawtick - data.readtick(i)) / DATA_WIDTH;

            y1 = Y0 + H - H*((value - data.min(var))/ vspread);
            if (CURVE_MODE == 0)
                thickLineRGBA(gRenderer, x0, y0, x1, y1, LINE_WIDTH, color.r, color.g, color.b, color.a);
            else if (CURVE_MODE == 1) {
                SDL_SetRenderDrawColor( gRenderer, color.r, color.g, color.b, color.a );
                for (int i=0; i<LINE_WIDTH; i++) {
                    SDL_RenderDrawLine(gRenderer, x0, y0+i, x1, y1+i);
                    SDL_RenderDrawLine(gRenderer, x0+i, y0, x1+i, y1);
                }
            }
            x0 = x1; y0 = y1;
            if (x0 < X0) break;
        }
    }

}


void plot_heart_alert(SDL_Renderer *gRenderer, int idev, int quadrant = 0) {
    SDL_Rect tRect = getScreenQuadrant(screen, quadrant);
    int W = tRect.w, H = tRect.h, X0 = tRect.x, Y0 = tRect.y;


    if (pdata[idev].connStatus() == 1) {
        SDL_Rect alertRect = { X0, Y0, W, H };
        int alertred = 40 + ((alertphase < 80) ? alertphase : 160 - alertphase);
        SDL_SetRenderDrawColor( gRenderer, alertred, 0x00, 0x00, 0xFF );
        SDL_RenderFillRect(gRenderer, &alertRect);

        if (AUDIO && (beepcooldown == 0)) {
            SDL_QueueAudio(AUDIOdev, beepsound.buffer, beepsound.length);
            SDL_PauseAudioDevice(AUDIOdev, 0);
            beepcooldown = 300;
        }
    }

    if (pdata[idev].connStatus() == 2) {
        SDL_Rect alertRect = { X0, Y0, W, H };
        int alertred = 40 + ((alertphase < 80) ? alertphase : 160 - alertphase);
        SDL_SetRenderDrawColor( gRenderer, alertred & 0xFF, alertred / 2, 0x00, 0xFF );
        SDL_RenderFillRect(gRenderer, &alertRect);

        if (AUDIO && (beepcooldown == 0)) {
            SDL_QueueAudio(AUDIOdev, beepsound.buffer, beepsound.length);
            SDL_PauseAudioDevice(AUDIOdev, 0);
            beepcooldown = 800;
        }
    }
}




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
        hrvmod = 60.0 / clamp(ls[i].hrv, 0.01, 5.0);
        if (((ls[i].hrv < 5.0) && (ls[i].hrv > 0.01)) || (!FILTER_BAD)) { // This IF will filter out obviously bad values
//            curve_index[i]++;
//            curve_write(readtick[i], curve_index[i], MAX_DATALEN, SDL_GetTicks64());
//            curve_write(heart[i], curve_index[i], MAX_DATALEN, hrvmod);
//            curve_write(scl[i], curve_index[i], MAX_DATALEN, sclmod);

//            if (hrvmod > 100.0) hrvmod = 100.0 + (hrvmod - 100.0) * 0.2;

//            if (hrvmod > 100.0) hrvmod = 100.0 + pow(hrvmod - 100, 0.9);   //log(hrvmod - 98.0) / log(1.0);

            float k = 0.03;
            if (fabs(hrvmod - 60.0) < 3.0) pdata[i].heartConnected = (1.0 - 0.01) * pdata[i].heartConnected;      // Tend toward zero
            else pdata[i].heartConnected = k + (1.0 - k) * pdata[i].heartConnected;
            if (fabs(sclmod) < 1) pdata[i].sclConnected = (1.0 - k) * pdata[i].sclConnected;               // Tend toward zero
            else pdata[i].sclConnected = k + (1.0 - k) * pdata[i].sclConnected;


            pdata[i].readtick(-1) = SDL_GetTicks64();
//            printf("%li ", pdata[i].readtick(0));

            pdata[i].value(patient_data::scl, -1) = sclmod;
            if (pdata[i].connStatus() == 0) pdata[i].value(patient_data::heart, -1) = 60.5;
            else if (pdata[i].connStatus() == 1) pdata[i].value(patient_data::heart, -1) = (hrvmod - 60.5) * 5.0 + 60.5;      // amplify
            else if (pdata[i].connStatus() == 2) pdata[i].value(patient_data::heart, -1) = hrvmod;
            else if (pdata[i].connStatus() == 3) pdata[i].value(patient_data::heart, -1) = hrvmod;

            if (pdata[i].connStatus() > 1) {
                double dpulse, output = 60.0 / clamp(ls[i].hrv, 0.3, 5.0);
//                hrvmod = apply_sos_section(hrvmod, sos, SOSState_hrv);
                for (size_t i = 0; i < sos.size(); ++i) {
                    output = apply_sos_section(output, sos[i], states[i]);
                }
                pdata[i].value(patient_data::pulse, -1) = float(output);
                dpulse = pdata[i].value(patient_data::pulse, -1) - pdata[i].value(patient_data::pulse, 0);
                pdata[i].value(patient_data::dpulse, -1) = dpulse;

                pdata[i].value(patient_data::heart, -1) = hrvmod;
                pdata[i].value(patient_data::scl, -1) = sclmod;

                if ((pdata[i].value(patient_data::dpulse, 0) * dpulse < 0)
                    && (pdata[i].value(patient_data::pulse, -1) < 0.0)) {

                    if (pdata[i].pulseTicks.size() > 0) {
                        pdata[i].hrQueue.push_back(60000.0 / float(pdata[i].readtick(-1) - pdata[i].pulseTicks.back()));
                        if (pdata[i].hrQueue.size() > 10) pdata[i].hrQueue.pop_front();
                        float hrSum = 0.0;
                        for (auto qi = pdata[i].hrQueue.begin(); qi != pdata[i].hrQueue.end(); qi++) {
                            hrSum += *qi;
                        }
//                        pdata[i].heartRate = pdata[i].hrQueue.back();
                        pdata[i].heartRate = hrSum / (pdata[i].hrQueue.size());
                        while (pdata[i].heartRate > 200.0) pdata[i].heartRate -= 100.0; 
                   }

                    pdata[i].pulseTicks.push_back(pdata[i].readtick(-1));
                    if (pdata[i].pulseTicks.size() > 10) pdata[i].pulseTicks.pop_front();
                }

//                if (output > pdata[i].scl(0) + 10.0) pdata[i].scl(-1) += 60;
            }
            pdata[i].index += 1;
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
        screen = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
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
    SDL_Surface *textSfc = TTF_RenderText_Solid (gFont, "ABC", white);
    SDL_Texture* textTex = SDL_CreateTextureFromSurface(gRenderer, textSfc);

    SDL_Surface *htitlesfc[4], *stitlesfc[4], *hrsfc[4];
    SDL_Texture *htitletex[4], *stitletex[4], *hrtex[4] = {NULL, NULL, NULL, NULL};
    SDL_Surface *numbersfc[4];      // For showing numerical output
    SDL_Texture *numbertex[4] = { NULL, NULL, NULL, NULL };      //

    // Set up SDL textures for titles
    for (i=0; i<numdev; i++) {
        htitlesfc[i] = TTF_RenderText_Solid (gFont, RED_CURVENAME, red);
        htitletex[i] = SDL_CreateTextureFromSurface(gRenderer, htitlesfc[i]);

        stitlesfc[i] = TTF_RenderText_Solid (gFont, BLUE_CURVENAME, blue);
        stitletex[i] = SDL_CreateTextureFromSurface(gRenderer, stitlesfc[i]);

        SDL_FreeSurface(htitlesfc[i]);      // These not needed after creating textures
        SDL_FreeSurface(stitlesfc[i]);      //
    }

    // Create the SDL threads that will read values
    int tid[4]; // to pass thread functions
    SDL_Thread *threads[4];
    for (int i=0; i<numdev; i++) {
        tid[i] = i;
        threads[i] = SDL_CreateThread(read_thread, "read_thread", &tid[i]);
    }


    // START MAIN LOOP
    SDL_Event e;
    int frame = 0, disp;
    int systolic[4] = { 0, 0, 0, 0 }, diastolic[4] = { 0, 0, 0, 0 };
    int textw, texth;   // TEMPORARY
    while (!quit) {

        SDL_SetRenderDrawColor( gRenderer, 0x00, 0x00, 0x00, 0xFF );
        SDL_RenderClear(gRenderer);

        for (i=0; i<numdev; i++) {
            numdev > 1? disp = i+1 : disp = 0;

            SDL_Rect tRect = getScreenQuadrant(screen, disp);
            int W = tRect.w, H = tRect.h, X0 = tRect.x, Y0 = tRect.y;
            int FULLSCREEN = (numdev <= 2) ? 1 : 0;  // TODO Check if this should be disp instead of i

            if (lightstone_get_count(lsdev[0]) < numdev) {
                // Then check which device is disconnected, and react
//                SDL_Rect dstRect = { X0+40, Y0+40, W/(3*(FULLSCREEN+1)), H/(3*(FULLSCREEN+1)) };
//                SDL_RenderCopy(gRenderer, htitletex[i], NULL, &dstRect);
//                lightstone_close(lsdev[0]);
//                numdev--;
            }
            if (lightstone_get_count(lsdev[0]) > numdev) {
                // Then reconnect a device somewhere
//                lightstone_open(lsdev[0], 0);
            }


            plot_heart_alert(gRenderer, i, disp);

            plot_curve(gRenderer, pdata[i], patient_data::heart, red, disp);                // Plot the red curve
            if (PLOT_BLUE) plot_curve(gRenderer, pdata[i], patient_data::scl, blue, disp);     // and maybe the blue curve
            if (PLOT_BLUE) plot_curve(gRenderer, pdata[i], patient_data::pulse, green, disp);     // and maybe the blue curve
            if (PLOT_BLUE) plot_curve(gRenderer, pdata[i], patient_data::dpulse, white, disp);     // and maybe the blue curve

            // RENDER TEXT LABEL(S) FOR CURVES
            SDL_Rect dstRect = { X0+10, Y0+10, W/(3*(FULLSCREEN+1)), H/(3*(FULLSCREEN+1)) };
            SDL_RenderCopy(gRenderer, htitletex[i], NULL, &dstRect);
            if (PLOT_BLUE) {
                dstRect = { X0+10, Y0+10+H/(3*(FULLSCREEN+1)), W/(3*(FULLSCREEN+1)), H/(3*(FULLSCREEN+1)) };
                SDL_RenderCopy(gRenderer, stitletex[i], NULL, &dstRect);
            }

            if (PLOT_HR) {
                char hrnumbers[4][80];
                if (frame % 70 == 0) {
                    if (pdata[i].connStatus() == 0) sprintf(hrnumbers[i], " ");
                    if (pdata[i].connStatus() == 1) sprintf(hrnumbers[i], "--");
                    if (pdata[i].connStatus() == 2) sprintf(hrnumbers[i], "%i", int(pdata[i].heartRate));
                    if (pdata[i].connStatus() == 3) sprintf(hrnumbers[i], "%i", int(pdata[i].heartRate));

                    // Update the texture
                    if (NULL != hrtex[i]) SDL_DestroyTexture(hrtex[i]);
                    hrsfc[i] = TTF_RenderText_Solid (gFont, hrnumbers[i], red);
                    hrtex[i] = SDL_CreateTextureFromSurface(gRenderer, hrsfc[i]);
                    SDL_FreeSurface(hrsfc[i]);

                }
                SDL_QueryTexture(hrtex[i], NULL, NULL, &textw, &texth);
                dstRect = { X0+W - (textw/texth)*H/7, Y0+H*0/7, (textw/texth)*H/7, H/7 };
                SDL_RenderCopy(gRenderer, hrtex[i], NULL, &dstRect);
            }

            if (PLOT_DIA_SYS) {
                // Also plot numerical output
                char numbers[80];
                if (frame % 70 == 0) {
                    if (pdata[i].connStatus() == 0) sprintf(numbers, " ");
                    if (pdata[i].connStatus() == 1) sprintf(numbers, "- / -");
                    if (pdata[i].connStatus() == 2) sprintf(numbers, "%i / %i",
                            int(pdata[i].value(patient_data::heart, 0) + 5) % 10 + 70 + frame % 10, int(pdata[i].value(patient_data::heart, 0)) % 20 + 40);
                    if (pdata[i].connStatus() == 3) sprintf(numbers, "%i / %i",
                            int(pdata[i].value(patient_data::scl, 0)) + 35 + frame % 10, int(pdata[i].value(patient_data::scl, 0)));

                    // Update the texture
                    if (NULL != numbertex[i]) SDL_DestroyTexture(numbertex[i]);
                    numbersfc[i] = TTF_RenderText_Solid (gFont, numbers, red);
                    numbertex[i] = SDL_CreateTextureFromSurface(gRenderer, numbersfc[i]);
                    SDL_FreeSurface(numbersfc[i]);

                }
//            sprintf(numbers, "%i / %i", 100 + int(pdata[i].heartConnected*100.0), 100 + int(pdata[i].sclConnected*100.0));
                SDL_QueryTexture(numbertex[i], NULL, NULL, &textw, &texth);
                dstRect = { X0+W - (textw/texth)*H/7, Y0+H*6/7, (textw/texth)*H/7, H/7 };
                SDL_RenderCopy(gRenderer, numbertex[i], NULL, &dstRect);
            }
        } // loop over lightstone devices

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

                case SDLK_s:        // Toggle plotting the blue curve
                    PLOT_BLUE = !PLOT_BLUE;
                    break;

                case SDLK_f:        // Toggle plotting the blue curve
                    FILTER_BAD = !FILTER_BAD;
                    break;

                case SDLK_m:        // Switch curve plotting mode
                    CURVE_MODE = (CURVE_MODE + 1) % 2;
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

// Test noise
//        curve_append(heart_data, MAX_DATALEN, 60 + 5*(frame%10) + (frame%3)*8 + ((frame>>3)%9)*2);
//        curve_append(scl_data, MAX_DATALEN, 1.0 + float(frame%7)/3 + float((frame>>4)%19)/7);

        frame++;
        if (++alertphase >160) alertphase = 0;
        if (beepcooldown > 0) beepcooldown--;
    }

    for (i=0; i<numdev; i++) {
        SDL_DestroyTexture(htitletex[i]);   // Dunno if clearing needed but here is
        SDL_DestroyTexture(stitletex[i]);
    }

    if (gFont) TTF_CloseFont (gFont);
	SDL_DestroyWindow( window );
	SDL_Quit();

    for (i=0; i<numdev; i++) lightstone_delete(lsdev[i]);

	return 0;
}
