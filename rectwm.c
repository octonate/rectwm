#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <stdbool.h>
#include <unistd.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <X11/XF86keysym.h>
#include <X11/Xutil.h>

struct Client {
    Window win;
    struct Client *next, *prev;
};

struct KeyBind {
    uint mod;
    KeySym key;
    void (*func)(void **args);
    void **args;
};

struct Client *focusedClient;
Display *dpy;
Window root;
bool isNoClient = True;
XButtonEvent mouse;

void exec(void *args[]) {
    if (fork() == 0) {
        execvp((char *)args[0], (char **)args);
    }
}

void exitRectwm() {
    XCloseDisplay(dpy);
}

void clientFocus() {
    if (isNoClient) return;
    XRaiseWindow(dpy, focusedClient->win);
    XSetInputFocus(dpy, focusedClient->win, RevertToParent, CurrentTime);
}

void clientNext() {
    if (isNoClient) return;
    focusedClient = focusedClient->next;
    clientFocus();
}

void clientPrev() {
    if (isNoClient) return;
    focusedClient = focusedClient->prev;
    clientFocus();
}

void clientAdd(Window win) {
    struct Client *newClient = malloc(sizeof(struct Client));
    newClient->win = win;
    if (isNoClient) {
        focusedClient = newClient;
        focusedClient->next = focusedClient;
        focusedClient->prev = focusedClient;
    } else {
        newClient->next = focusedClient->next;
        newClient->prev = focusedClient;
        focusedClient->next->prev = newClient;
        focusedClient->next= newClient;

        focusedClient = newClient;
    }
    isNoClient = False;
}

void clientDelete(Window win) {
    if (isNoClient) return;
    struct Client *client = focusedClient;

    for (client = focusedClient; client->win != win; client = client->next) {
        if (client->next == focusedClient) {
            return;
        }
    }
    client->prev->next = client->next;
    client->next->prev = client->prev;
    if (client == focusedClient) {
        focusedClient = client->prev;

        if (focusedClient == client) {
            isNoClient = True;
        }
    }
    free(client);
    clientFocus();
}

void clientClose() {
    if (isNoClient) return;

    XEvent killEv;
    killEv.xclient.type = ClientMessage;
    killEv.xclient.window = focusedClient->win;
    killEv.xclient.message_type = XInternAtom(dpy, "WM_PROTOCOLS", True);
    killEv.xclient.format = 32;
    killEv.xclient.data.l[0] = XInternAtom(dpy, "WM_DELETE_WINDOW", True);
    killEv.xclient.data.l[1] = CurrentTime;
    XSendEvent(dpy, focusedClient->win, False, NoEventMask, &killEv);
}

void clientForceKill() {
    if (isNoClient) return;
    XKillClient(dpy, focusedClient->win);
}

/* KEY BIND CONFIGURATION */

// change this to whatever mod key you want
#define MOD_MASK Mod4Mask

const struct KeyBind keyBinds[] = {
//  { mod key,  key,              address of function,     function arguments },
//                                (must be void func)      (must cast as void ptr array if any arguments)
    { MOD_MASK,           XK_w,           &clientClose,              0 },
    { MOD_MASK|ShiftMask, XK_w,           &clientForceKill,          0 },
    { MOD_MASK,           XK_h,           &clientPrev,               0 },
    { MOD_MASK,           XK_l,           &clientNext,               0 },
    { MOD_MASK,           XK_q,           &exitRectwm,               0 },

    { MOD_MASK, XK_Return,                &exec,          (void *[]){"alacritty", 0} },
    { MOD_MASK, XK_b,                     &exec,          (void *[]){"firefox",   0} },
    { MOD_MASK, XK_d,                     &exec,          (void *[]){"discord",   0} },

    { 0,        XF86XK_MonBrightnessUp,   &exec,          (void *[]){"brightnessctl", "set", "+5",           0} },
    { 0,        XF86XK_MonBrightnessDown, &exec,          (void *[]){"brightnessctl", "set", "5-",           0} },

    { 0,        XF86XK_AudioLowerVolume,  &exec,          (void *[]){"amixer", "-D", "pulse", "sset", "Master", "5%-", 0} },
    { 0,        XF86XK_AudioRaiseVolume,  &exec,          (void *[]){"amixer", "-D", "pulse", "sset", "Master", "5%+", 0} },
};

void loop() {
    XEvent ev;

    while(1) {
        XNextEvent(dpy, &ev);

        switch (ev.type) {
        case ConfigureRequest:
            XConfigureWindow(dpy, ev.xconfigurerequest.window, ev.xconfigurerequest.value_mask, &(XWindowChanges) {
                .x = 0,
                .y = 0,
                .width = ev.xconfigurerequest.width,
                .height = ev.xconfigurerequest.height,
                .border_width = ev.xconfigurerequest.border_width,
                .sibling = ev.xconfigurerequest.above,
                .stack_mode = ev.xconfigurerequest.detail });
            break;

        case MapRequest:
            XSelectInput(dpy, ev.xmaprequest.window, StructureNotifyMask | EnterWindowMask);
            clientAdd(ev.xmaprequest.window);
            XResizeWindow(dpy, ev.xmaprequest.window, DisplayWidth(dpy, DefaultScreen(dpy)), DisplayHeight(dpy, DefaultScreen(dpy)));
            XMapWindow(dpy, ev.xmaprequest.window);
            clientFocus();
            break;
            
        case KeyPress:
            for (uint i = 0; i < sizeof(keyBinds) / sizeof(struct KeyBind); ++i) {
                if (keyBinds[i].key == XkbKeycodeToKeysym(dpy, ev.xkey.keycode, 0, 0)) {
                    keyBinds[i].func(keyBinds[i].args);
                }
            }
            break;
            
        case UnmapNotify:
        case DestroyNotify:
            clientDelete(ev.xdestroywindow.window);
            break;

        case MotionNotify:
            mouse = ev.xbutton;
            if (isNoClient) break;
            XMoveWindow(dpy, focusedClient->win, mouse.x_root, mouse.y_root);
            break;

        }
    }
}

int main() {
    dpy = XOpenDisplay(NULL);
    if (dpy == NULL) {
        err(1, "Can't open display :(\n");
    }

    XSetErrorHandler(0);

    root = DefaultRootWindow(dpy);

    for (uint i = 0; i < sizeof(keyBinds) / sizeof(struct KeyBind); ++i) {
        XGrabKey(dpy, XKeysymToKeycode(dpy, keyBinds[i].key), keyBinds[i].mod, root, true, GrabModeAsync, GrabModeAsync);
    }
    XGrabButton(dpy, 1, MOD_MASK, root, true, ButtonPressMask | ButtonReleaseMask | PointerMotionMask, GrabModeAsync, GrabModeAsync, 0, 0);
    XSelectInput(dpy, root, SubstructureNotifyMask | SubstructureRedirectMask); 
    XDefineCursor(dpy, root, XCreateFontCursor(dpy, 68));

    loop();
    
    return 0;
}
