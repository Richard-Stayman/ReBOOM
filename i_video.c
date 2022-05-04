// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id: i_video.c,v 1.12 1998/05/03 22:40:35 killough Exp $
//
//  BOOM, a modified and improved DOOM engine
//  Copyright (C) 1999 by
//  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 
//  02111-1307, USA.
//
// DESCRIPTION:
//      DOOM graphics stuff
//
//-----------------------------------------------------------------------------

#ifdef UNIX
#include "SDL2/SDL.h"
#include "SDL2/SDL_image.h"
#else
#include "SDL.h" // haleyjd
#include "SDL_image.h"
#endif

#include "z_zone.h"  /* memory allocation wrappers -- killough */
#include "doomstat.h"
#include "v_video.h"
#include "d_main.h"
#include "m_bbox.h"
#include "st_stuff.h"
#include "m_argv.h"
#include "w_wad.h"
#include "r_draw.h"
#include "am_map.h"
#include "m_menu.h"
#include "wi_stuff.h"
#include "i_video.h"

SDL_Surface *sdlscreen;

// [FG] rendering window, renderer, intermediate ARGB frame buffer and texture

SDL_Window *screen;
SDL_Renderer *renderer;
SDL_Surface *argbbuffer;
SDL_Texture *texture;

/////////////////////////////////////////////////////////////////////////////
//
// JOYSTICK                                                  // phares 4/3/98
//
/////////////////////////////////////////////////////////////////////////////

extern int usejoystick;
extern int joystickpresent;
extern int joy_x,joy_y;
extern int joy_b1,joy_b2,joy_b3,joy_b4;

// I_JoystickEvents() gathers joystick data and creates an event_t for
// later processing by G_Responder().

int joystickSens_x;
int joystickSens_y;

extern SDL_Joystick *sdlJoystick;

void I_JoystickEvents(void)
{
   // haleyjd 04/15/02: SDL joystick support

   event_t event;
   int joy_b1, joy_b2, joy_b3, joy_b4;
   int joy_b5, joy_b6, joy_b7, joy_b8;
   Sint16 joy_x, joy_y;
   
   if(!joystickpresent || !usejoystick || !sdlJoystick)
      return;
   
   SDL_JoystickUpdate(); // read the current joystick settings
   event.type = ev_joystick;
   event.data1 = 0;
   
   // read the button settings
   if((joy_b1 = SDL_JoystickGetButton(sdlJoystick, 0)))
      event.data1 |= 1;
   if((joy_b2 = SDL_JoystickGetButton(sdlJoystick, 1)))
      event.data1 |= 2;
   if((joy_b3 = SDL_JoystickGetButton(sdlJoystick, 2)))
      event.data1 |= 4;
   if((joy_b4 = SDL_JoystickGetButton(sdlJoystick, 3)))
      event.data1 |= 8;
   if((joy_b5 = SDL_JoystickGetButton(sdlJoystick, 4)))
      event.data1 |= 16;
   if((joy_b6 = SDL_JoystickGetButton(sdlJoystick, 5)))
      event.data1 |= 32;
   if((joy_b7 = SDL_JoystickGetButton(sdlJoystick, 6)))
      event.data1 |= 64;
   if((joy_b8 = SDL_JoystickGetButton(sdlJoystick, 7)))
      event.data1 |= 128;
   
   // Read the x,y settings. Convert to -1 or 0 or +1.
   joy_x = SDL_JoystickGetAxis(sdlJoystick, 0);
   joy_y = SDL_JoystickGetAxis(sdlJoystick, 1);
   
   if(joy_x < -joystickSens_x)
      event.data2 = -1;
   else if(joy_x > joystickSens_x)
      event.data2 = 1;
   else
      event.data2 = 0;

   if(joy_y < -joystickSens_y)
      event.data3 = -1;
   else if(joy_y > joystickSens_y)
      event.data3 = 1;
   else
      event.data3 = 0;
   
   // post what you found
   
   D_PostEvent(&event);
}


//
// I_StartFrame
//
void I_StartFrame(void)
{
   static boolean firstframe = true;
   
   // haleyjd 02/23/04: turn mouse event processing on
   if(firstframe)
   {
      SDL_EventState(SDL_MOUSEMOTION, SDL_ENABLE);
      firstframe = false;
   }
   
   I_JoystickEvents(); // Obtain joystick data                 phares 4/3/98
}

/////////////////////////////////////////////////////////////////////////////
//
// END JOYSTICK                                              // phares 4/3/98
//
/////////////////////////////////////////////////////////////////////////////

// haleyjd 10/08/05: Chocolate DOOM application focus state code added

// Grab the mouse? (int type for config code)
int grabmouse = 1;

// Flag indicating whether the screen is currently visible:
// when the screen isnt visible, don't render the screen
boolean screenvisible;
static boolean window_focused;
boolean fullscreen;
int page_flip;     // killough 8/15/98: enables page flipping
static int in_page_flip;

//
// MouseShouldBeGrabbed
//
// haleyjd 10/08/05: From Chocolate DOOM, fairly self-explanatory.
//
static boolean MouseShouldBeGrabbed(void)
{
   // if the window doesnt have focus, never grab it
   if(!window_focused)
      return false;
   
   // always grab the mouse when full screen (dont want to 
   // see the mouse pointer)
   if(fullscreen)
      return true;
   
   // if we specify not to grab the mouse, never grab
   if(!grabmouse)
      return false;
   
   // when menu is active or game is paused, release the mouse 
   if(menuactive || paused)
      return false;
   
   // only grab mouse when playing levels (but not demos)
   return (gamestate == GS_LEVEL) && !demoplayback;
}

// [FG] mouse grabbing from Chocolate Doom 3.0

static void SetShowCursor(boolean show)
{
   // When the cursor is hidden, grab the input.
   // Relative mode implicitly hides the cursor.
   SDL_SetRelativeMouseMode(!show);
   SDL_GetRelativeMouseState(NULL, NULL);
}

// 
// UpdateGrab
//
// haleyjd 10/08/05: from Chocolate DOOM
//
static void UpdateGrab(void)
{
   static boolean currently_grabbed = false;
   boolean grab;
   
   grab = MouseShouldBeGrabbed();
   
   if(grab && !currently_grabbed)
   {
      SetShowCursor(false);
   }
   
   if(!grab && currently_grabbed)
   {
      int screen_w, screen_h;

      SetShowCursor(true);

      // When releasing the mouse from grab, warp the mouse cursor to
      // the bottom-right of the screen. This is a minimally distracting
      // place for it to appear - we may only have released the grab
      // because we're at an end of level intermission screen, for
      // example.

      SDL_GetWindowSize(screen, &screen_w, &screen_h);
      SDL_WarpMouseInWindow(screen, screen_w - 16, screen_h - 16);
      SDL_GetRelativeMouseState(NULL, NULL);
   }
   
   currently_grabbed = grab;   
}

//
// Keyboard routines
// By Lee Killough
// Based only a little bit on Chi's v0.2 code
//

extern void I_InitKeyboard();      // i_system.c

static const int scancode_translate_table[] = SCANCODE_TO_KEYS_ARRAY;

/////////////////////////////////////////////////////////////////////////////////
// Keyboard handling

//
//  Translates the key currently in key
//

static int I_TranslateKey(SDL_Keysym* key)
{
   int scancode = key->scancode;

   switch (scancode)
    {
        case SDL_SCANCODE_LCTRL:
        case SDL_SCANCODE_RCTRL:
            return KEYD_RCTRL;

        case SDL_SCANCODE_LSHIFT:
        case SDL_SCANCODE_RSHIFT:
            return KEYD_RSHIFT;

        case SDL_SCANCODE_LALT:
            return KEYD_LALT;

        case SDL_SCANCODE_RALT:
            return KEYD_RALT;

        default:
            if (scancode >= 0 && scancode < arrlen(scancode_translate_table))
            {
                return scancode_translate_table[scancode];
            }
            else
            {
                return 0;
            }
    }

}

int I_ScanCode2DoomCode (int a)
{
   // haleyjd
   return a;
}

// Automatic caching inverter, so you don't need to maintain two tables.
// By Lee Killough

int I_DoomCode2ScanCode (int a)
{
   // haleyjd
   return a;
}

// Bit mask of mouse button state.
static unsigned int mouse_button_state = 0;

static void UpdateMouseButtonState(unsigned int button, boolean on)
{
    static event_t event;

    if (button < SDL_BUTTON_LEFT || button > 5)
    {
        return;
    }

    // Note: button "0" is left, button "1" is right,
    // button "2" is middle for Doom.  This is different
    // to how SDL sees things.

    switch (button)
    {
        case SDL_BUTTON_LEFT:
            button = 0;
            break;

        case SDL_BUTTON_RIGHT:
            button = 1;
            break;

        case SDL_BUTTON_MIDDLE:
            button = 2;
            break;

        default:
            // SDL buttons are indexed from 1.
            --button;
            break;
    }

    // Turn bit representing this button on or off.

    if (on)
    {
        mouse_button_state |= (1 << button);
    }
    else
    {
        mouse_button_state &= ~(1 << button);
    }

    // Post an event with the new button state.

    event.type = ev_mouse;
    event.data1 = mouse_button_state;
    event.data2 = event.data3 = 0;
    D_PostEvent(&event);
}

static void MapMouseWheelToButtons(SDL_MouseWheelEvent *wheel)
{
    // SDL2 distinguishes button events from mouse wheel events.
    // We want to treat the mouse wheel as two buttons, as per
    // SDL1
    static event_t up, down;
    int button;

    if (wheel->y <= 0)
    {   // scroll down
        button = 4;
    }
    else
    {   // scroll up
        button = 3;
    }

    // post a button down event
    mouse_button_state |= (1 << button);
    down.type = ev_mouse;
    down.data1 = mouse_button_state;
    down.data2 = down.data3 = 0;
    D_PostEvent(&down);

    // post a button up event
    mouse_button_state &= ~(1 << button);
    up.type = ev_mouse;
    up.data1 = mouse_button_state;
    up.data2 = up.data3 = 0;
    D_PostEvent(&up);
}

static void I_HandleMouseEvent(SDL_Event *sdlevent)
{
    switch (sdlevent->type)
    {
        case SDL_MOUSEBUTTONDOWN:
            UpdateMouseButtonState(sdlevent->button.button, true);
            break;

        case SDL_MOUSEBUTTONUP:
            UpdateMouseButtonState(sdlevent->button.button, false);
            break;

        case SDL_MOUSEWHEEL:
            MapMouseWheelToButtons(&(sdlevent->wheel));
            break;

        default:
            break;
    }
}

static void I_HandleKeyboardEvent(SDL_Event *sdlevent)
{
    // XXX: passing pointers to event for access after this function
    // has terminated is undefined behaviour
    event_t event;

    switch (sdlevent->type)
    {
        case SDL_KEYDOWN:
            event.type = ev_keydown;
            event.data1 = I_TranslateKey(&sdlevent->key.keysym);
/*
            event.data2 = GetLocalizedKey(&sdlevent->key.keysym);
            event.data3 = GetTypedChar(&sdlevent->key.keysym);
*/
            if (event.data1 != 0)
            {
                D_PostEvent(&event);
            }
            break;

        case SDL_KEYUP:
            event.type = ev_keyup;
            event.data1 = I_TranslateKey(&sdlevent->key.keysym);

            // data2/data3 are initialized to zero for ev_keyup.
            // For ev_keydown it's the shifted Unicode character
            // that was typed, but if something wants to detect
            // key releases it should do so based on data1
            // (key ID), not the printable char.

            event.data2 = 0;
            event.data3 = 0;

            if (event.data1 != 0)
            {
                D_PostEvent(&event);
            }
            break;

        default:
            break;
    }
}

// [FG] window event handling from Chocolate Doom 3.0

static void HandleWindowEvent(SDL_WindowEvent *event)
{
    switch (event->event)
    {
        // Don't render the screen when the window is minimized:

        case SDL_WINDOWEVENT_MINIMIZED:
            screenvisible = false;
            break;

        case SDL_WINDOWEVENT_MAXIMIZED:
        case SDL_WINDOWEVENT_RESTORED:
            screenvisible = true;
            break;

        // Update the value of window_focused when we get a focus event
        //
        // We try to make ourselves be well-behaved: the grab on the mouse
        // is removed if we lose focus (such as a popup window appearing),
        // and we dont move the mouse around if we aren't focused either.

        case SDL_WINDOWEVENT_FOCUS_GAINED:
            window_focused = true;
            break;

        case SDL_WINDOWEVENT_FOCUS_LOST:
            window_focused = false;
            break;

        default:
            break;
    }
}

// [FG] fullscreen toggle from Chocolate Doom 3.0

static boolean ToggleFullScreenKeyShortcut(SDL_Keysym *sym)
{
    Uint16 flags = (KMOD_LALT | KMOD_RALT);
#if defined(__APPLE__)
    flags |= (KMOD_LGUI | KMOD_RGUI);
#endif
    return sym->scancode == SDL_SCANCODE_RETURN && (sym->mod & flags) != 0;
}

// [FG] window size when returning from fullscreen mode
static int window_width, window_height;

static void I_ToggleFullScreen(void)
{
    unsigned int flags = 0;

    fullscreen = !fullscreen;

    if (fullscreen)
    {
        SDL_GetWindowSize(screen, &window_width, &window_height);
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }

    SDL_SetWindowFullscreen(screen, flags);

    if (!fullscreen)
    {
        SDL_SetWindowSize(screen, window_width, window_height);
    }
}

// killough 3/22/98: rewritten to use interrupt-driven keyboard queue

extern int usemouse;

void I_GetEvent(void)
{
   SDL_Event sdlevent;

   SDL_PumpEvents();

    while (SDL_PollEvent(&sdlevent))
    {
        switch (sdlevent.type)
        {
            case SDL_KEYDOWN:
                if (ToggleFullScreenKeyShortcut(&sdlevent.key.keysym))
                {
                    I_ToggleFullScreen();
                    break;
                }
                // deliberate fall-though

            case SDL_KEYUP:
                I_HandleKeyboardEvent(&sdlevent);
                break;

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            case SDL_MOUSEWHEEL:
                if (usemouse && window_focused)
                {
                    I_HandleMouseEvent(&sdlevent);
                }
                break;

            case SDL_QUIT:
                exit(0);
                break;

            case SDL_WINDOWEVENT:
                if (sdlevent.window.windowID == SDL_GetWindowID(screen))
                {
                    HandleWindowEvent(&sdlevent.window);
                }
                break;

            default:
                break;
        }
    }
}

//
// Read the change in mouse state to generate mouse motion events
//
// This is to combine all mouse movement for a tic into one mouse
// motion event.

static const float mouse_acceleration = 2.0;
static const int mouse_threshold = 10;

int AccelerateMouse(int val)
{
    if (val < 0)
        return -AccelerateMouse(-val);

    if (val > mouse_threshold)
    {
        return (int)((val - mouse_threshold) * mouse_acceleration + mouse_threshold);
    }
    else
    {
        return val;
    }
}

static void I_ReadMouse(void)
{
    int x, y;
    event_t ev;

    SDL_GetRelativeMouseState(&x, &y);

    if (x != 0 || y != 0)
    {
        ev.type = ev_mouse;
        ev.data1 = mouse_button_state;
        ev.data2 = AccelerateMouse(x);
        ev.data3 = -AccelerateMouse(y);

        // XXX: undefined behaviour since event is scoped to
        // this function
        D_PostEvent(&ev);
    }
}

//
// I_StartTic
//

void I_StartTic()
{
    I_GetEvent();

    if (usemouse && window_focused)
    {
        I_ReadMouse();
    }
}

//
// I_UpdateNoBlit
//

void I_UpdateNoBlit (void)
{
}

int use_vsync;     // killough 2/8/98: controls whether vsync is called
static int in_graphics_mode;
static int useaspect, actualheight; // [FG] aspect ratio correction
int integer_scaling; // [FG] force integer scales

void I_FinishUpdate(void)
{
   if (noblit || !in_graphics_mode)
      return;

   // haleyjd 10/08/05: from Chocolate DOOM:

   UpdateGrab();

   // draws little dots on the bottom of the screen
   if(devparm)
   {
      static int lasttic;
      byte *s = screens[0];
      
      int i = I_GetTime();
      int tics = i - lasttic;
      lasttic = i;
      if (tics > 20)
      {
         tics = 20;
      }
         for (i=0 ; i<tics*2 ; i+=2)
            s[(SCREENHEIGHT-1)*SCREENWIDTH + i] = 0xff;
         for ( ; i<20*2 ; i+=2)
            s[(SCREENHEIGHT-1)*SCREENWIDTH + i] = 0x0;
   }

   SDL_BlitSurface(sdlscreen, NULL, argbbuffer, NULL);

   SDL_UpdateTexture(texture, NULL, argbbuffer->pixels, argbbuffer->pitch);

   SDL_RenderClear(renderer);
   SDL_RenderCopy(renderer, texture, NULL, NULL);
   SDL_RenderPresent(renderer);
}

//
// I_ReadScreen
//

void I_ReadScreen(byte *scr)
{
   int size = SCREENWIDTH*SCREENHEIGHT;

   // haleyjd
   memcpy(scr, *screens, size);
}

//
// killough 10/98: init disk icon
//

int disk_icon;

static void I_InitDiskFlash(void)
{
}

//
// killough 10/98: draw disk icon
//

void I_BeginRead(void)
{
}

//
// killough 10/98: erase disk icon
//

void I_EndRead(void)
{
}

void I_SetPalette(byte *palette)
{
   // haleyjd
   int i;
   SDL_Color colors[256];
   
   if(!in_graphics_mode)             // killough 8/11/98
      return;
   
   for(i = 0; i < 256; ++i)
   {
      colors[i].r = gammatable[usegamma][*palette++];
      colors[i].g = gammatable[usegamma][*palette++];
      colors[i].b = gammatable[usegamma][*palette++];
   }
   
   SDL_SetPaletteColors(sdlscreen->format->palette, colors, 0, 256);
}

void I_ShutdownGraphics(void)
{
   if(in_graphics_mode)  // killough 10/98
   {
      UpdateGrab();      
      in_graphics_mode = false;
   }
}

boolean I_WritePNGfile(char* filename)
{
  SDL_Rect rect = {0};
  SDL_PixelFormat *format;
  SDL_Surface *png_surface;
  int pitch;
  byte *pixels;
  boolean ret;

  // [FG] native PNG pixel format
  const uint32_t png_format = SDL_PIXELFORMAT_RGB24;
  format = SDL_AllocFormat(png_format);

  // [FG] adjust cropping rectangle if necessary
  SDL_GetRendererOutputSize(renderer, &rect.w, &rect.h);
  if (useaspect || integer_scaling)
  {
    int temp;
    if (integer_scaling)
    {
      int temp1, temp2, scale;
      temp1 = rect.w;
      temp2 = rect.h;
      scale = ReBOOMMathMin(rect.w / SCREENWIDTH, rect.h / actualheight);

      rect.w = SCREENWIDTH * scale;
      rect.h = actualheight * scale;

      rect.x = (temp1 - rect.w) / 2;
      rect.y = (temp2 - rect.h) / 2;
    }
    else
    if (rect.w * actualheight > rect.h * SCREENWIDTH)
    {
      temp = rect.w;
      rect.w = rect.h * SCREENWIDTH / actualheight;
      rect.x = (temp - rect.w) / 2;
    }
    else
    if (rect.h * SCREENWIDTH > rect.w * actualheight)
    {
      temp = rect.h;
      rect.h = rect.w * actualheight / SCREENWIDTH;
      rect.y = (temp - rect.h) / 2;
    }
  }

  // [FG] allocate memory for screenshot image
  pitch = rect.w * format->BytesPerPixel;
  pixels = malloc(rect.h * pitch);
  SDL_RenderReadPixels(renderer, &rect, format->format, pixels, pitch);
  png_surface = SDL_CreateRGBSurfaceWithFormatFrom(pixels, rect.w, rect.h, format->BitsPerPixel, pitch, png_format);

  ret = (IMG_SavePNG(png_surface, filename) == 0);

  SDL_FreeSurface(png_surface);
  SDL_FreeFormat(format);
  free(pixels);

  return ret;
}

extern boolean setsizeneeded;

extern void I_InitKeyboard();

int cfg_scalefactor; // haleyjd 05/11/09: scale factor in config
int cfg_aspectratio; // haleyjd 05/11/09: aspect ratio correction

// haleyjd 05/11/09: true if called from I_ResetScreen
static boolean changeres = false;

static void I_InitGraphicsMode(void)
{
   static boolean firsttime = true;
   
   // haleyjd
   int v_w = SCREENWIDTH;
   int v_h = SCREENHEIGHT;
   int flags = 0;
   int scalefactor = cfg_scalefactor;

   // [FG] SDL2
   uint32_t pixel_format;
   int video_display;
   SDL_DisplayMode mode;

   if(firsttime)
   {
      I_InitKeyboard();
      firsttime = false;
   }

   // haleyjd 10/09/05: from Chocolate DOOM
   // mouse grabbing   
   if(M_CheckParm("-grabmouse"))
      grabmouse = 1;
   else if(M_CheckParm("-nograbmouse"))
      grabmouse = 0;

   // [FG] window flags
   flags |= SDL_WINDOW_RESIZABLE;
   flags |= SDL_WINDOW_ALLOW_HIGHDPI;

   // haleyjd: fullscreen support
   if (M_CheckParm("-window"))
   {
       fullscreen = false;
   }
   if (M_CheckParm("-fullscreen") || fullscreen)
   {
      fullscreen = true; // 5/11/09: forgotten O_O
      flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
   }

   useaspect = cfg_aspectratio;
   if(M_CheckParm("-aspect"))
      useaspect = true;

   actualheight = useaspect ? (6 * v_h / 5) : v_h;

   // [FG] create rendering window

   if (screen == NULL)
   {
      screen = SDL_CreateWindow(NULL,
                                // centered window
                                SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                v_w, v_h, flags);

      if (screen == NULL)
      {
         I_Error("Error creating window for video startup: %s",
                 SDL_GetError());
      }
   }

   SDL_SetWindowMinimumSize(screen, v_w, actualheight);

   // [FG] window size when returning from fullscreen mode
   window_width = scalefactor * v_w;
   window_height = scalefactor * actualheight;

   if (!(flags & SDL_WINDOW_FULLSCREEN_DESKTOP))
   {
      SDL_SetWindowSize(screen, SUPERMAX_SCREENWIDTH, SUPERMAX_SCREENHEIGHT);
      SDL_SetWindowPosition(screen, 200, 80);
   }

   pixel_format = SDL_GetWindowPixelFormat(screen);
   video_display = SDL_GetWindowDisplayIndex(screen);

   flags = 0;

   if (SDL_GetCurrentDisplayMode(video_display, &mode) != 0)
   {
      I_Error("Could not get display mode for video display #%d: %s",
              video_display, SDL_GetError());
   }

   if (page_flip && use_vsync && !timingdemo && mode.refresh_rate > 0)
   {
       flags |= SDL_RENDERER_PRESENTVSYNC;
   }

   // [FG] page_flip = !force_software_renderer
   if (!page_flip)
   {
       flags |= SDL_RENDERER_SOFTWARE;
       flags &= ~SDL_RENDERER_PRESENTVSYNC;
       use_vsync = false;
   }

   if (renderer != NULL)
   {
      SDL_DestroyRenderer(renderer);
      texture = NULL;
   }

   renderer = SDL_CreateRenderer(screen, -1, flags);

   if (renderer == NULL && page_flip)
   {
       flags |= SDL_RENDERER_SOFTWARE;
       flags &= ~SDL_RENDERER_PRESENTVSYNC;

       renderer = SDL_CreateRenderer(screen, -1, flags);

       if (renderer != NULL)
       {
           // remove any special flags
           use_vsync = page_flip = false;
       }
   }

   if (renderer == NULL)
   {
      I_Error("Error creating renderer for screen window: %s",
              SDL_GetError());
   }

   SDL_RenderSetLogicalSize(renderer, v_w, actualheight);

   SDL_RenderSetIntegerScale(renderer, integer_scaling);

   SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
   SDL_RenderClear(renderer);
   SDL_RenderPresent(renderer);

   // [FG] create paletted frame buffer

   if (sdlscreen != NULL)
   {
      SDL_FreeSurface(sdlscreen);
      sdlscreen = NULL;
   }

   if (sdlscreen == NULL)
   {
      sdlscreen = SDL_CreateRGBSurface(0,
                                       v_w, v_h, 8,
                                       0, 0, 0, 0);
      SDL_FillRect(sdlscreen, NULL, 0);

      // [FG] screen buffer
      screens[0] = sdlscreen->pixels;
   }

   // [FG] create intermediate ARGB frame buffer

   if (argbbuffer != NULL)
   {
      SDL_FreeSurface(argbbuffer);
      argbbuffer = NULL;
   }

   if (argbbuffer == NULL)
   {
      unsigned int rmask, gmask, bmask, amask;
      int unused_bpp;

      SDL_PixelFormatEnumToMasks(pixel_format, &unused_bpp,
                                 &rmask, &gmask, &bmask, &amask);
      argbbuffer = SDL_CreateRGBSurface(0,
                                        v_w, v_h, 32,
                                        rmask, gmask, bmask, amask);
      SDL_FillRect(argbbuffer, NULL, 0);
   }

   // [FG] create texture

   if (texture != NULL)
   {
      SDL_DestroyTexture(texture);
   }

   SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

   texture = SDL_CreateTexture(renderer,
                               pixel_format,
                               SDL_TEXTUREACCESS_STREAMING,
                               v_w, v_h);

   V_Init();

   SDL_SetWindowTitle(screen, BOOM_WINDOW_TEXT);

   UpdateGrab();

   in_graphics_mode = 1;
   in_page_flip = true;
   setsizeneeded = true;
   
   I_InitDiskFlash();        // Initialize disk icon 
     
   I_SetPalette(W_CacheLumpName("PLAYPAL",PU_CACHE));
}

void I_ResetScreen(void)
{
   if(!in_graphics_mode)
   {
      setsizeneeded = true;
      V_Init();
      return;
   }

   I_ShutdownGraphics();     // Switch out of old graphics mode
   
   changeres = true; // haleyjd 05/11/09
   in_page_flip = page_flip;

   I_InitGraphicsMode();     // Switch to new graphics mode
   
   changeres = false;
   
   if(automapactive)
      AM_Start();             // Reset automap dimensions
   
   ST_Start();               // Reset palette
   
   if(gamestate == GS_INTERMISSION)
   {
      WI_DrawBackground();
      V_CopyRect(0, 0, 1, SCREENWIDTH, SCREENHEIGHT, 0, 0, 0);
   }
}

void I_InitGraphics(void)
{
  static int firsttime = 1;

  if(!firsttime)
    return;

  firsttime = 0;

  //
  // enter graphics mode
  //

  atexit(I_ShutdownGraphics);

  I_InitGraphicsMode();    // killough 10/98
}
