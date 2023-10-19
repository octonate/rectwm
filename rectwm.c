#include <X11/X.h>
#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdint.h>

#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>

#define MOD_MASK Mod4Mask

static Display *dpy;
static Window root;

typedef struct Client {
    Window win;
    struct Client *next, *prev;
} Client;

static Client *focusedClient;
static bool isNoClient = True;


void clientFocus() {
    if (isNoClient) return;
    XRaiseWindow(dpy, focusedClient->win);
    XSetInputFocus(dpy, focusedClient->win, RevertToParent, CurrentTime);
}

void clientAdd(Window win) {
    Client *newClient = malloc(sizeof (Client));
    newClient->win = win;
    if (isNoClient) {
        focusedClient = newClient;
        focusedClient->next = focusedClient;
        focusedClient->prev = focusedClient;
    }
    else {
        newClient->next = focusedClient;
        newClient->prev = focusedClient->prev;
        focusedClient->prev->next = newClient;
        focusedClient->prev = newClient;

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
    switch (keysym) {
        case XK_Return:
            if (fork() == 0) {
                execvp("alacritty", NULL);
            }
            break;
        case XK_b:
            if (fork() == 0) {
                execvp("firefox", NULL);
            }
            break;
        case XK_q:
            XCloseDisplay(dpy);
            break;
        case XK_l:
            clientNext();
            break;
        case XK_h:
            clientPrev();
            break;
        case XK_w:
            if (isNoClient) break;
            XKillClient(dpy, focusedClient->win);
            break;
    }
}

void handleDestroyNotify(XDestroyWindowEvent *ev) {
    clientDelete(ev->window);
    clientFocus();
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


    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_Return), MOD_MASK, root, true, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_q), MOD_MASK, root, true, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_l), MOD_MASK, root, true, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_h), MOD_MASK, root, true, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_w), MOD_MASK, root, true, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, XKeysymToKeycode(dpy, XK_b), MOD_MASK, root, true, GrabModeAsync, GrabModeAsync);
    XSelectInput(dpy, root, SubstructureNotifyMask | SubstructureRedirectMask); 
    XDefineCursor(dpy, root, XCreateFontCursor(dpy, 68));


    loop();
    
    return 0;
}
