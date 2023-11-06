#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdint.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <X11/XF86keysym.h>

// change to whatever mod key you want
#define MOD_MASK Mod4Mask

typedef struct Client {
    Window win;
    struct Client *next, *prev;
} Client;

typedef struct {
    uint mod;
    KeySym key;
    void (*func)(void **args);
    void **args;
} KeyBind;

static Client *focusedClient;
static Display *dpy;
static Window root;
static bool isNoClient = True;

void exec(void *args[]) {
    if (fork() == 0) {
        execvp((char *)args[0], (char **)args);
    }
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
    Client *newClient = malloc(sizeof (Client));
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
    Client *client = focusedClient;

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

void clientKill() {
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

/* KEY BIND CONFIGURATION */
static KeyBind keyBinds[] = {
//  { mod key,  key,              address of function,     function arguments },
//                                (must be void func)      (must cast as void ptr array)
    { MOD_MASK, XK_w,                     &clientKill,               0 },
    { MOD_MASK, XK_h,                     &clientPrev,               0 },
    { MOD_MASK, XK_l,                     &clientNext,               0 },

    { MOD_MASK, XK_q,             (void *)&XCloseDisplay, (void *[]){&dpy,        0} },
    { MOD_MASK, XK_Return,                &exec,          (void *[]){"alacritty", 0} },
    { MOD_MASK, XK_b,                     &exec,          (void *[]){"firefox",   0} },

    { 0,        XF86XK_MonBrightnessUp,   &exec,          (void *[]){"brightnessctl", "set", "+5", 0} },
    { 0,        XF86XK_MonBrightnessDown, &exec,          (void *[]){"brightnessctl", "set", "5-", 0} },
};

void handleConfigureRequest(XConfigureRequestEvent *ev) {
    XConfigureWindow(dpy, ev->window, ev->value_mask, &(XWindowChanges) {
        .x = ev->x,
        .y = ev->y,
        .width = ev->width,
        .height = ev->height,
        .border_width = ev->border_width,
        .sibling = ev->above,
        .stack_mode = ev->detail
    });
}

void handleMapRequest(XMapRequestEvent *ev) {
    XSelectInput(dpy, ev->window, StructureNotifyMask|EnterWindowMask);
    clientAdd(ev->window);
    XResizeWindow(dpy, ev->window, DisplayWidth(dpy, DefaultScreen(dpy)), DisplayHeight(dpy, DefaultScreen(dpy)));
    XMapWindow(dpy, ev->window);
    clientFocus();
}

void handleKeyPress(XKeyEvent *ev) {
    KeySym keysym = XkbKeycodeToKeysym(dpy, ev->keycode, 0, 0);

    for (uint i = 0; i < sizeof (keyBinds) / sizeof (KeyBind); ++i) {
        if (keyBinds[i].key == keysym) {
            keyBinds[i].func(keyBinds[i].args);
        }
    }
}

void handleDestroyNotify(XDestroyWindowEvent *ev) {
    clientDelete(ev->window);
}

void loop() {
    XEvent ev;

    while(1) {
        XNextEvent(dpy, &ev);

        switch (ev.type) {
            case ConfigureRequest:
                handleConfigureRequest(&ev.xconfigurerequest);
                break;
            case MapRequest:
                handleMapRequest(&ev.xmaprequest);
                break;
            case KeyPress:
                handleKeyPress(&ev.xkey);
                break;
            case DestroyNotify:
                handleDestroyNotify(&ev.xdestroywindow);
                break;
        }
    }
}

int main() {
    dpy = XOpenDisplay(NULL);
    if (dpy == NULL) {
        err(1, "Can't open display :(");
    }

    XSetErrorHandler(0);

    root = DefaultRootWindow(dpy);

    for (uint i = 0; i < sizeof (keyBinds) / sizeof (KeyBind); ++i) {
        XGrabKey(dpy, XKeysymToKeycode(dpy, keyBinds[i].key), keyBinds[i].mod, root, true, GrabModeAsync, GrabModeAsync);
    }
    XSelectInput(dpy, root, SubstructureNotifyMask | SubstructureRedirectMask); 
    XDefineCursor(dpy, root, XCreateFontCursor(dpy, 68));

    loop();
    
    return 0;
}
