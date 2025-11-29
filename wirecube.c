#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <math.h>

int sin_table[256];

void init_sin_table() {
    for (int i = 0; i < 256; i++) {
        double angle = (i * 6.283185307) / 256.0;
        sin_table[i] = (int)(sin(angle) * 4096.0);
    }
}

int fast_sin(int angle) {
    return sin_table[angle & 255];
}

int fast_cos(int angle) {
    return sin_table[(angle + 64) & 255];
}

int main() {
    Display* d = XOpenDisplay(NULL);
    if (!d) return 1;
    
    init_sin_table();
    
    int s = DefaultScreen(d);
    int w = 800;
    int h = 600;
    Window root = RootWindow(d, s);
    
    XSetWindowAttributes attrs;
    attrs.background_pixel = BlackPixel(d, s);
    attrs.event_mask = KeyPressMask | StructureNotifyMask | EnterWindowMask | LeaveWindowMask;
    attrs.backing_store = Always;
    
    Window win = XCreateWindow(d, root, 100, 100, w, h, 0, CopyFromParent, 
                               InputOutput, CopyFromParent, 
                               CWBackPixel | CWEventMask | CWBackingStore, &attrs);
    XStoreName(d, win, "WireCube");
    XMapWindow(d, win);
    XSync(d, False);

    Pixmap backbuffer = XCreatePixmap(d, win, w, h, DefaultDepth(d, s));
    GC gc = XCreateGC(d, backbuffer, 0, NULL);
    XSetGraphicsExposures(d, gc, False);
    Colormap cmap = DefaultColormap(d, s);

    unsigned long pixels[256];
    XColor color;
    for (int i = 0; i < 256; i++) {
        double hue = (i / 256.0) * 6.0;
        double c = 1.0;
        double x = c * (1.0 - fabs(fmod(hue, 2.0) - 1.0));
        double r, g, b;
        if (hue < 1.0) { r = c; g = x; b = 0; }
        else if (hue < 2.0) { r = x; g = c; b = 0; }
        else if (hue < 3.0) { r = 0; g = c; b = x; }
        else if (hue < 4.0) { r = 0; g = x; b = c; }
        else if (hue < 5.0) { r = x; g = 0; b = c; }
        else { r = c; g = 0; b = x; }
        color.red = (unsigned short)(r * 65535);
        color.green = (unsigned short)(g * 65535);
        color.blue = (unsigned short)(b * 65535);
        XAllocColor(d, cmap, &color);
        pixels[i] = color.pixel;
    }

    Pixmap pm = XCreatePixmap(d, win, 1, 1, 1);
    XColor dummy;
    Cursor invisible;
    dummy.red = dummy.green = dummy.blue = 0;
    invisible = XCreatePixmapCursor(d, pm, pm, &dummy, &dummy, 0, 0);
    int cursor_hidden = 0;

    Atom wmDelete = XInternAtom(d, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(d, win, &wmDelete, 1);

    Atom wmState = XInternAtom(d, "_NET_WM_STATE", False);
    Atom wmFullscreen = XInternAtom(d, "_NET_WM_STATE_FULLSCREEN", False);
    int is_fullscreen = 0;

    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 16666666;

    srand((unsigned int)time(NULL));

    int angX = rand() & 255;
    int angY = rand() & 255;
    int angZ = rand() & 255;
    int velAX = (rand() % 3) - 1;
    int velAY = (rand() % 3) - 1;
    int velAZ = (rand() % 3) - 1;
    if (velAX == 0) velAX = 1;
    if (velAY == 0) velAY = 1;
    if (velAZ == 0) velAZ = 1;

    int size = (int)(50.0 * 4096.0);
    int posX = (w / 2) * 4096;
    int posY = (h / 2) * 4096;
    
    int angle = rand() & 255;
    int speed = (int)((3 + (rand() % 3)) * 4096.0);
    int velX = (fast_cos(angle) * speed) >> 12;
    int velY = (fast_sin(angle) * speed) >> 12;

    int running = 1;
    int color_offset = 0;
    
    int frame_count = 0;
    struct timespec last_fps_time;
    clock_gettime(CLOCK_MONOTONIC, &last_fps_time);

    int cube[8][3] = {
        {-4096,-4096,-4096}, {4096,-4096,-4096},
        {4096,4096,-4096}, {-4096,4096,-4096},
        {-4096,-4096,4096}, {4096,-4096,4096},
        {4096,4096,4096}, {-4096,4096,4096}
    };

    int edges[12][2] = {
        {0,1},{1,2},{2,3},{3,0},
        {4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7}
    };

    int verts2D[8][2];
    unsigned long black = BlackPixel(d, s);
    
    XSelectInput(d, win, KeyPressMask | StructureNotifyMask | EnterWindowMask | LeaveWindowMask);

    while (running) {
        while (XPending(d) > 0) {
            XEvent e;
            XNextEvent(d, &e);
            if (e.type == ConfigureNotify) {
                w = e.xconfigure.width;
                h = e.xconfigure.height;
                XFreePixmap(d, backbuffer);
                backbuffer = XCreatePixmap(d, win, w, h, DefaultDepth(d, s));
                XFreeGC(d, gc);
                gc = XCreateGC(d, backbuffer, 0, NULL);
                XSetGraphicsExposures(d, gc, False);
                posX = (w / 2) * 4096;
                posY = (h / 2) * 4096;
                continue;
            }
            else if (e.type == KeyPress) {
                KeySym ks = XLookupKeysym(&e.xkey, 0);
                if (ks == XK_Escape) {
                    running = 0;
                }
                else if (ks == XK_f || ks == XK_F) {
                    XEvent cm;
                    memset(&cm, 0, sizeof(cm));
                    cm.xclient.type = ClientMessage;
                    cm.xclient.message_type = wmState;
                    cm.xclient.window = win;
                    cm.xclient.format = 32;
                    cm.xclient.data.l[0] = is_fullscreen ? 0 : 1;
                    cm.xclient.data.l[1] = wmFullscreen;
                    cm.xclient.data.l[2] = 0;
                    XSendEvent(d, root, False, SubstructureRedirectMask | SubstructureNotifyMask, &cm);
                    is_fullscreen = !is_fullscreen;
                }
            }
            else if (e.type == ClientMessage) {
                if ((Atom)e.xclient.data.l[0] == wmDelete) running = 0;
            }
            else if (e.type == EnterNotify) {
                if (!cursor_hidden) {
                    XDefineCursor(d, win, invisible);
                    cursor_hidden = 1;
                }
            }
            else if (e.type == LeaveNotify) {
                if (cursor_hidden) {
                    XUndefineCursor(d, win);
                    cursor_hidden = 0;
                }
            }
        }

        angX = (angX + velAX) & 255;
        angY = (angY + velAY) & 255;
        angZ = (angZ + velAZ) & 255;

        int sX = fast_sin(angX);
        int cX = fast_cos(angX);
        int sY = fast_sin(angY);
        int cY = fast_cos(angY);
        int sZ = fast_sin(angZ);
        int cZ = fast_cos(angZ);

        int minX = 1000000;
        int minY = 1000000;
        int maxX = -1000000;
        int maxY = -1000000;

        for (int i = 0; i < 8; i++) {
            int x = cube[i][0];
            int y = cube[i][1];
            int z = cube[i][2];

            int rx = ((x * cY) >> 12) + ((z * sY) >> 12);
            int rz = ((-x * sY) >> 12) + ((z * cY) >> 12);
            int ry = ((y * cX) >> 12) - ((rz * sX) >> 12);
            rz = ((y * sX) >> 12) + ((rz * cX) >> 12);
            int nx = ((rx * cZ) >> 12) - ((ry * sZ) >> 12);
            int ny = ((rx * sZ) >> 12) + ((ry * cZ) >> 12);

            int sx = (((nx * size) >> 12) + posX) >> 12;
            int sy = (((ny * size) >> 12) + posY) >> 12;

            verts2D[i][0] = sx;
            verts2D[i][1] = sy;
            
            if (sx < minX) minX = sx;
            if (sx > maxX) maxX = sx;
            if (sy < minY) minY = sy;
            if (sy > maxY) maxY = sy;
        }

        if (minX <= 0 && velX < 0) {
            velX = -velX;
            posX += (-minX) * 4096;
        }
        if (maxX >= w && velX > 0) {
            velX = -velX;
            posX -= (maxX - w) * 4096;
        }
        if (minY <= 0 && velY < 0) {
            velY = -velY;
            posY += (-minY) * 4096;
        }
        if (maxY >= h && velY > 0) {
            velY = -velY;
            posY -= (maxY - h) * 4096;
        }

        posX += velX;
        posY += velY;

        XSetForeground(d, gc, black);
        XFillRectangle(d, backbuffer, gc, 0, 0, w, h);

        for (int i = 0; i < 12; i++) {
            int a = edges[i][0];
            int b = edges[i][1];
            int color_idx = (color_offset + (i * 21)) & 255;
            XSetForeground(d, gc, pixels[color_idx]);
            XDrawLine(d, backbuffer, gc, 
                     verts2D[a][0], verts2D[a][1],
                     verts2D[b][0], verts2D[b][1]);
        }

        color_offset = (color_offset + 1) & 255;

        XCopyArea(d, backbuffer, win, gc, 0, 0, w, h, 0, 0);
        XSync(d, False);
        
        frame_count++;
        struct timespec current_time;
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        long elapsed_sec = current_time.tv_sec - last_fps_time.tv_sec;
        long elapsed_nsec = current_time.tv_nsec - last_fps_time.tv_nsec;
        double elapsed = elapsed_sec + elapsed_nsec / 1000000000.0;
        
        if (elapsed >= 1.0) {
            double fps = frame_count / elapsed;
            printf("\rFPS: %.1f", fps);
            fflush(stdout);
            frame_count = 0;
            last_fps_time = current_time;
        }
        
        nanosleep(&ts, NULL);
    }

    if (cursor_hidden) XUndefineCursor(d, win);
    XFreeCursor(d, invisible);
    XFreePixmap(d, pm);
    XFreePixmap(d, backbuffer);
    XFreeGC(d, gc);
    XDestroyWindow(d, win);
    XCloseDisplay(d);
    return 0;
}
