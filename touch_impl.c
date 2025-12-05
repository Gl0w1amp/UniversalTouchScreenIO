#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#ifndef WINVER
#define WINVER 0x0A00
#endif

#include "touch_impl.h"
#include "dprintf.h"
#include <windows.h>
#include <process.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#ifndef WM_POINTERUPDATE
#define WM_POINTERUPDATE 0x0245
#define WM_POINTERDOWN   0x0246
#define WM_POINTERUP     0x0247
#endif

typedef struct {
    int x;
    int y;
} Point;

typedef struct {
    Point* points;
    int count;
    int offset_x;
    int offset_y;
    int sensor_id; // 0-33
} SensorPolygon;

typedef struct PointerNode {
    UINT32 id;
    int x;
    int y;
    struct PointerNode* next;
} PointerNode;

static SensorPolygon sensors[34];
static PointerNode* active_pointers_head = NULL;
static uint64_t current_touch_state_1p = 0;
static uint64_t current_touch_state_2p = 0;
static bool is_2p_mode = false;
static bool is_debug_visible = false;
static CRITICAL_SECTION state_lock;
static HWND overlay_hwnd = NULL;
static int overlay_width = 0;
static int overlay_height = 0;
static int overlay_left = 0;
static int overlay_top = 0;

// Polygon Data Helper
static void init_polygon(int id, int ox, int oy, int count, const int* coords) {
    sensors[id].sensor_id = id;
    sensors[id].offset_x = ox;
    sensors[id].offset_y = oy;
    sensors[id].count = count;
    sensors[id].points = (Point*)malloc(sizeof(Point) * count);
    for (int i = 0; i < count; i++) {
        sensors[id].points[i].x = coords[i * 2];
        sensors[id].points[i].y = coords[i * 2 + 1];
    }
}

// Point in Polygon
static bool point_in_polygon(int x, int y, SensorPolygon* poly) {
    bool inside = false;
    int count = poly->count;
    Point* pts = poly->points;
    int ox = poly->offset_x;
    int oy = poly->offset_y;
    
    int x0 = ox + pts[count - 1].x;
    int y0 = oy + pts[count - 1].y;
    
    for (int i = 0; i < count; i++) {
        int x1 = ox + pts[i].x;
        int y1 = oy + pts[i].y;
        
        if ((y1 > y) != (y0 > y)) {
            int xInt = x1 + (y - y1) * (x0 - x1) / (y0 - y1);
            if (xInt > x) inside = !inside;
        }
        x0 = x1;
        y0 = y1;
    }
    return inside;
}

// Initialize all polygons
static void init_sensors() {
    // A1 (0)
    static const int p_a1[] = {150,28, 245,65, 360,133, 208,338, 145,338, 49,297, 0,249, 35,0};
    init_polygon(0, 786, 11, 8, p_a1);

    // A2 (1)
    static const int p_a2[] = {261,101, 303,195, 339,327, 91,362, 42,314, 0,219, 0,150, 202,0};
    init_polygon(1, 1091, 292, 8, p_a2);

    // A3 (2)
    static const int p_a3[] = {305,150, 269,246, 201,364, 0,213, 0,144, 41,48, 89,0, 337,34};
    init_polygon(2, 1092, 786, 8, p_a3);

    // A4 (3)
    static const int p_a4[] = {260,259, 167,301, 37,335, 0,83, 48,35, 144,0, 212,0, 364,200};
    init_polygon(3, 786, 1092, 8, p_a4);

    // A5 (4)
    static const int p_a5[] = {104,259, 197,301, 327,335, 363,83, 316,35, 220,0, 152,0, 0,201};
    init_polygon(4, 291, 1092, 8, p_a5);

    // A6 (5)
    static const int p_a6[] = {32,150, 68,246, 133,365, 333,214, 333,144, 296,48, 248,0, 0,35};
    init_polygon(5, 16, 785, 8, p_a6);

    // A7 (6)
    static const int p_a7[] = {78,101, 36,195, 0,327, 248,362, 297,314, 333,219, 333,151, 132,0};
    init_polygon(6, 16, 291, 8, p_a7);

    // A8 (7)
    static const int p_a8[] = {210,28, 115,65, 0,138, 153,338, 215,338, 311,297, 359,249, 324,0};
    init_polygon(7, 295, 11, 8, p_a8);

    // B1 (8)
    static const int p_b1[] = {0,78, 78,0, 209,55, 209,165, 180,195, 70,195, 0,130};
    init_polygon(8, 720, 346, 7, p_b1);

    // B2 (9)
    static const int p_b2[] = {117,209, 195,132, 140,0, 30,0, 0,30, 0,139, 65,209};
    init_polygon(9, 900, 511, 7, p_b2);

    // B3 (10)
    static const int p_b3[] = {120,0, 198,78, 140,208, 30,208, 0,180, 0,71, 65,0};
    init_polygon(10, 900, 721, 7, p_b3);

    // B4 (11)
    static const int p_b4[] = {0,112, 87,198, 208,140, 208,29, 177,0, 71,0, 0,65};
    init_polygon(11, 721, 901, 7, p_b4);

    // B5 (12)
    static const int p_b5[] = {208,112, 121,198, 0,140, 0,29, 31,0, 137,0, 208,65};
    init_polygon(12, 512, 901, 7, p_b5);

    // B6 (13)
    static const int p_b6[] = {78,0, 0,78, 58,208, 163,208, 193,180, 193,71, 133,0};
    init_polygon(13, 349, 721, 7, p_b6);

    // B7 (14)
    static const int p_b7[] = {82,209, 0,127, 55,0, 165,0, 195,30, 195,139, 137,209};
    init_polygon(14, 345, 511, 7, p_b7);

    // B8 (15)
    static const int p_b8[] = {209,78, 131,0, 0,55, 0,165, 29,195, 139,195, 209,130};
    init_polygon(15, 511, 346, 7, p_b8);

    // C1 (16)
    static const int p_c1[] = {0,0, 60,0, 140,80, 140,200, 60,280, 0,280, 0,0};
    init_polygon(16, 720, 583, 7, p_c1);

    // C2 (17)
    static const int p_c2[] = {141,280, 81,280, 0,199, 1,81, 81,0, 141,0, 141,280};
    init_polygon(17, 579, 583, 7, p_c2);

    // D1 (18)
    static const int p_d1[] = {0,5, 50,2, 100,0, 150,2, 200,5, 165,253, 100,188, 35,253};
    init_polygon(18, 620, 6, 8, p_d1);

    // D2 (19)
    static const int p_d2[] = {153,0, 187,32, 225,67, 259,104, 295,147, 96,297, 96,205, 0,205};
    init_polygon(19, 995, 144, 8, p_d2);

    // D3 (20)
    static const int p_d3[] = {248,0, 251,48, 253,100, 251,150, 247,199, 0,165, 65,100, 0,35};
    init_polygon(20, 1182, 620, 8, p_d3);

    // D4 (21)
    static const int p_d4[] = {292,151, 260,187, 225,225, 188,259, 151,291, 0,92, 92,92, 92,0};
    init_polygon(21, 1000, 1000, 8, p_d4);

    // D5 (22)
    static const int p_d5[] = {199,252, 151,255, 99,257, 49,255, 0,252, 34,0, 99,65, 164,0};
    init_polygon(22, 621, 1175, 8, p_d5);

    // D6 (23)
    static const int p_d6[] = {140,292, 104,260, 66,225, 32,188, 0,151, 199,0, 199,92, 291,92};
    init_polygon(23, 150, 1000, 8, p_d6);

    // D7 (24)
    static const int p_d7[] = {5,199, 2,151, 0,99, 2,49, 6,0, 253,34, 188,99, 253,164};
    init_polygon(24, 10, 620, 8, p_d7);

    // D8 (25)
    static const int p_d8[] = {0,140, 32,104, 67,66, 104,32, 145,0, 298,199, 200,199, 200,291};
    init_polygon(25, 149, 150, 8, p_d8);

    // E1 (26)
    static const int p_e1[] = {0,113, 113,0, 226,113, 113,226};
    init_polygon(26, 607, 195, 4, p_e1);

    // E2 (27)
    static const int p_e2[] = {0,0, 0,160, 160,160, 160,0, 0,0};
    init_polygon(27, 930, 350, 5, p_e2);

    // E3 (28)
    static const int p_e3[] = {0,113, 113,0, 226,113, 113,226};
    init_polygon(28, 1020, 607, 4, p_e3);

    // E4 (29)
    static const int p_e4[] = {0,0, 0,160, 160,160, 160,0, 0,0};
    init_polygon(29, 930, 930, 5, p_e4);

    // E5 (30)
    static const int p_e5[] = {0,113, 113,0, 226,113, 113,226};
    init_polygon(30, 607, 1013, 4, p_e5);

    // E6 (31)
    static const int p_e6[] = {0,0, 0,160, 160,160, 160,0, 0,0};
    init_polygon(31, 350, 930, 5, p_e6);

    // E7 (32)
    static const int p_e7[] = {0,113, 113,0, 226,113, 113,226};
    init_polygon(32, 200, 607, 4, p_e7);

    // E8 (33)
    static const int p_e8[] = {0,0, 0,160, 160,160, 160,0, 0,0};
    init_polygon(33, 350, 350, 5, p_e8);
}

static void check_point_against_sensors(double cx, double cy, uint64_t* state) {
    for (int i = 0; i < 34; i++) {
        if (sensors[i].points && point_in_polygon((int)cx, (int)cy, &sensors[i])) {
            *state |= (1ULL << i);
        }
    }
}

static void check_sensors_for_player(double offset_x, double offset_y, double scale, PointerNode* head, uint64_t* state) {
    PointerNode* curr = head;
    while (curr) {
        // Map screen coordinates to canvas
        double cx = (curr->x - offset_x) / scale;
        double cy = (curr->y - offset_y) / scale;

        // Check point
        check_point_against_sensors(cx, cy, state);

        // Radius checks (25px)
        double r = 25.0;
        int samples = 8;
        
        // Outer ring
        for (int i = 0; i < samples; i++) {
            double ang = (i * 2.0 * 3.1415926535) / samples;
            double px = cx + r * cos(ang);
            double py = cy + r * sin(ang);
            check_point_against_sensors(px, py, state);
        }

        // Inner ring
        double ri = r * 0.5;
        for (int i = 0; i < samples; i++) {
            double ang = (i * 2.0 * 3.1415926535) / samples;
            double px = cx + ri * cos(ang);
            double py = cy + ri * sin(ang);
            check_point_against_sensors(px, py, state);
        }

        curr = curr->next;
    }
}

static void recalc_touch_state() {
    EnterCriticalSection(&state_lock);
    current_touch_state_1p = 0;
    current_touch_state_2p = 0;
    
    if (!is_2p_mode) {
        // 1P Logic (Center)
        int target_w, target_h;
        int target_x, target_y;

        if (overlay_width * 16 > overlay_height * 9) {
            // Window is wider than 9:16 (e.g. Landscape), fit to height
            target_h = overlay_height;
            target_w = target_h * 9 / 16;
            target_y = 0;
            target_x = (overlay_width - target_w) / 2;
        } else {
            // Window is taller than 9:16, fit to width
            target_w = overlay_width;
            target_h = target_w * 16 / 9;
            target_x = 0;
            target_y = (overlay_height - target_h) / 2;
        }

        double scale = (double)target_w / 1440.0;
        double ring_size = 1440.0 * scale;
        
        double offset_x = (double)target_x;
        double offset_y = (double)target_y + (double)target_h - ring_size;

        check_sensors_for_player(offset_x, offset_y, scale, active_pointers_head, &current_touch_state_1p);
    } else {
        // 2P Logic: Treat as single 18:16 (9:8) area
        int target_w, target_h;
        int target_x, target_y;

        // 18:16 ratio = 1.125
        if (overlay_width * 16 > overlay_height * 18) {
            // Window is wider than 18:16, fit to height
            target_h = overlay_height;
            target_w = target_h * 18 / 16;
            target_y = 0;
            target_x = (overlay_width - target_w) / 2;
        } else {
            // Window is taller than 18:16, fit to width
            target_w = overlay_width;
            target_h = target_w * 16 / 18;
            target_x = 0;
            target_y = (overlay_height - target_h) / 2;
        }

        // Each player gets half width
        int p_w = target_w / 2;
        double scale = (double)p_w / 1440.0;
        double ring_size = 1440.0 * scale;

        // P1 (Left)
        double ox1 = (double)target_x;
        double oy1 = (double)target_y + (double)target_h - ring_size; // Bottom aligned
        check_sensors_for_player(ox1, oy1, scale, active_pointers_head, &current_touch_state_1p);

        // P2 (Right)
        double ox2 = (double)target_x + (double)p_w;
        double oy2 = oy1;
        check_sensors_for_player(ox2, oy2, scale, active_pointers_head, &current_touch_state_2p);
    }
    LeaveCriticalSection(&state_lock);
}

static void update_pointer(UINT32 id, int x, int y, bool down) {
    PointerNode** pp = &active_pointers_head;
    while (*pp) {
        if ((*pp)->id == id) {
            if (down) {
                (*pp)->x = x;
                (*pp)->y = y;
                recalc_touch_state();
                return;
            } else {
                // Remove
                PointerNode* temp = *pp;
                *pp = temp->next;
                free(temp);
                recalc_touch_state();
                return;
            }
        }
        pp = &(*pp)->next;
    }

    if (down) {
        PointerNode* new_node = (PointerNode*)malloc(sizeof(PointerNode));
        new_node->id = id;
        new_node->x = x;
        new_node->y = y;
        new_node->next = NULL;
        *pp = new_node; // Append to end
        recalc_touch_state();
    }
}

static LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_TIMER:
        {
            if (wParam == 1) {
                HWND hGame = FindWindow(NULL, "Sinmai");
                if (hGame) {
                    RECT rc;
                    GetWindowRect(hGame, &rc);
                    int w = rc.right - rc.left;
                    int h = rc.bottom - rc.top;
                    if (rc.left != overlay_left || rc.top != overlay_top || w != overlay_width || h != overlay_height) {
                        overlay_left = rc.left;
                        overlay_top = rc.top;
                        overlay_width = w;
                        overlay_height = h;
                        MoveWindow(hwnd, overlay_left, overlay_top, overlay_width, overlay_height, TRUE);
                        // Force repaint to update scaling
                        InvalidateRect(hwnd, NULL, TRUE);
                    }
                }
            }
            return 0;
        }
        case WM_LBUTTONDOWN:
        case WM_MOUSEMOVE:
        case WM_LBUTTONUP:
        {
            // Simulate mouse as pointer ID 1
            if (msg == WM_LBUTTONDOWN || (msg == WM_MOUSEMOVE && (wParam & MK_LBUTTON))) {
                POINT pt = { LOWORD(lParam), HIWORD(lParam) };
                // dprintf("Mouse Down/Move: %d, %d\n", pt.x, pt.y);
                update_pointer(1, pt.x, pt.y, true);
            } else if (msg == WM_LBUTTONUP) {
                POINT pt = { LOWORD(lParam), HIWORD(lParam) };
                // dprintf("Mouse Up: %d, %d\n", pt.x, pt.y);
                update_pointer(1, pt.x, pt.y, false);
            }
            return 0;
        }
        case WM_POINTERDOWN:
        case WM_POINTERUPDATE:
        case WM_POINTERUP:
        {
            UINT32 pointerId = GET_POINTERID_WPARAM(wParam);
            POINTER_INFO pointerInfo;
            if (GetPointerInfo(pointerId, &pointerInfo)) {
                POINT pt = pointerInfo.ptPixelLocation;
                ScreenToClient(hwnd, &pt);
                bool down = (msg != WM_POINTERUP);
                // dprintf("Touch: ID=%d X=%d Y=%d Down=%d\n", pointerId, (int)pt.x, (int)pt.y, down);
                update_pointer(pointerId, pt.x, pt.y, down);
            }
            return 0;
        }
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc;
            HBRUSH hBgBrush;
            RECT rc;

            hdc = BeginPaint(hwnd, &ps);
            
            // Draw semi-transparent background
            hBgBrush = CreateSolidBrush(RGB(0, 0, 0));
            GetClientRect(hwnd, &rc);
            FillRect(hdc, &rc, hBgBrush);
            DeleteObject(hBgBrush);

            if (is_debug_visible) {
                HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 255, 0)); // Green outline
                HPEN oldPen = SelectObject(hdc, hPen);
                HBRUSH hBrush = GetStockObject(NULL_BRUSH);
                HBRUSH oldBrush = SelectObject(hdc, hBrush);
                int i;

                if (!is_2p_mode) {
                    // 1. Calculate the 9:16 target area centered in the window
                    int target_w, target_h;
                    int target_x, target_y;

                    if (overlay_width * 16 > overlay_height * 9) {
                        target_h = overlay_height;
                        target_w = target_h * 9 / 16;
                        target_y = 0;
                        target_x = (overlay_width - target_w) / 2;
                    } else {
                        target_w = overlay_width;
                        target_h = target_w * 16 / 9;
                        target_x = 0;
                        target_y = (overlay_height - target_h) / 2;
                    }

                    // 2. Calculate ring scale and position (Bottom of the 9:16 area)
                    double scale = (double)target_w / 1440.0;
                    double ring_size = 1440.0 * scale;
                    
                    double offset_x = (double)target_x;
                    double offset_y = (double)target_y + (double)target_h - ring_size;

                    for (i = 0; i < 34; i++) {
                        if (sensors[i].count > 0) {
                            POINT pts[16];
                            int j;
                            for(j=0; j<sensors[i].count; j++) {
                                double raw_x = sensors[i].offset_x + sensors[i].points[j].x;
                                double raw_y = sensors[i].offset_y + sensors[i].points[j].y;
                                
                                pts[j].x = (LONG)(offset_x + raw_x * scale);
                                pts[j].y = (LONG)(offset_y + raw_y * scale);
                            }
                            Polygon(hdc, pts, sensors[i].count);
                            
                            // Draw ID
                            {
                                char buf[4];
                                sprintf_s(buf, 4, "%d", i);
                                TextOut(hdc, pts[0].x, pts[0].y, buf, (int)strlen(buf));
                            }
                        }
                    }
                } else {
                    // 2P Logic: Treat as single 18:16 (9:8) area
                    int target_w, target_h;
                    int target_x, target_y;

                    // 18:16 ratio = 1.125
                    if (overlay_width * 16 > overlay_height * 18) {
                        // Window is wider than 18:16, fit to height
                        target_h = overlay_height;
                        target_w = target_h * 18 / 16;
                        target_y = 0;
                        target_x = (overlay_width - target_w) / 2;
                    } else {
                        // Window is taller than 18:16, fit to width
                        target_w = overlay_width;
                        target_h = target_w * 16 / 18;
                        target_x = 0;
                        target_y = (overlay_height - target_h) / 2;
                    }

                    // Each player gets half width
                    int p_w = target_w / 2;
                    double scale = (double)p_w / 1440.0;
                    double ring_size = 1440.0 * scale;

                    // P1 (Left)
                    double ox1 = (double)target_x;
                    double oy1 = (double)target_y + (double)target_h - ring_size;

                    // P2 (Right)
                    double ox2 = (double)target_x + (double)p_w;
                    double oy2 = oy1;

                    // Draw P1
                    for (i = 0; i < 34; i++) {
                        if (sensors[i].count > 0) {
                            POINT pts[16];
                            int j;
                            for(j=0; j<sensors[i].count; j++) {
                                double raw_x = sensors[i].offset_x + sensors[i].points[j].x;
                                double raw_y = sensors[i].offset_y + sensors[i].points[j].y;
                                
                                pts[j].x = (LONG)(ox1 + raw_x * scale);
                                pts[j].y = (LONG)(oy1 + raw_y * scale);
                            }
                            Polygon(hdc, pts, sensors[i].count);
                            // Draw ID
                            {
                                char buf[8];
                                sprintf_s(buf, 8, "1P-%d", i);
                                TextOut(hdc, pts[0].x, pts[0].y, buf, (int)strlen(buf));
                            }
                        }
                    }

                    // Draw P2
                    for (i = 0; i < 34; i++) {
                        if (sensors[i].count > 0) {
                            POINT pts[16];
                            int j;
                            for(j=0; j<sensors[i].count; j++) {
                                double raw_x = sensors[i].offset_x + sensors[i].points[j].x;
                                double raw_y = sensors[i].offset_y + sensors[i].points[j].y;
                                
                                pts[j].x = (LONG)(ox2 + raw_x * scale);
                                pts[j].y = (LONG)(oy2 + raw_y * scale);
                            }
                            Polygon(hdc, pts, sensors[i].count);
                            // Draw ID
                            {
                                char buf[8];
                                sprintf_s(buf, 8, "2P-%d", i);
                                TextOut(hdc, pts[0].x, pts[0].y, buf, (int)strlen(buf));
                            }
                        }
                    }
                }

                SelectObject(hdc, oldPen);
                SelectObject(hdc, oldBrush);
                DeleteObject(hPen);
            }

            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_NCHITTEST:
            return HTCLIENT;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static unsigned int __stdcall touch_window_thread(void* arg) {
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = OverlayWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "MaiTouchOverlay";
    RegisterClassEx(&wc);

    // Find Sinmai window
    HWND hGame = NULL;
    while ((hGame = FindWindow(NULL, "Sinmai")) == NULL) {
        Sleep(1000);
    }

    RECT rcGame;
    GetWindowRect(hGame, &rcGame);
    overlay_width = rcGame.right - rcGame.left;
    overlay_height = rcGame.bottom - rcGame.top;
    overlay_left = rcGame.left;
    overlay_top = rcGame.top;

    // Create Overlay
    overlay_hwnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_LAYERED, // Removed WS_EX_TRANSPARENT
        "MaiTouchOverlay", "TouchOverlay",
        WS_POPUP | WS_VISIBLE,
        overlay_left, overlay_top, overlay_width, overlay_height,
        NULL, NULL, wc.hInstance, NULL
    );

    SetLayeredWindowAttributes(overlay_hwnd, 0, is_debug_visible ? 128 : 1, LWA_ALPHA); // Semi-transparent if debug
    
    // Start a timer to track the game window position
    SetTimer(overlay_hwnd, 1, 100, NULL);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

static bool is_initialized = false;

void touch_overlay_init(void) {
    if (is_initialized) return;
    is_initialized = true;

    InitializeCriticalSection(&state_lock);
    init_sensors();
    _beginthreadex(NULL, 0, touch_window_thread, NULL, 0, NULL);
}

void touch_overlay_update(void) {
    // Check if window moved?
}

void touch_overlay_enable_2p(bool enable) {
    EnterCriticalSection(&state_lock);
    is_2p_mode = enable;
    LeaveCriticalSection(&state_lock);
    if (overlay_hwnd) {
        InvalidateRect(overlay_hwnd, NULL, TRUE);
    }
}

void touch_overlay_enable_debug(bool enable) {
    is_debug_visible = enable;
    if (overlay_hwnd) {
        SetLayeredWindowAttributes(overlay_hwnd, 0, is_debug_visible ? 128 : 1, LWA_ALPHA);
        InvalidateRect(overlay_hwnd, NULL, TRUE);
    }
}

void touch_overlay_get_state(uint8_t player_index, uint8_t state[7]) {
    EnterCriticalSection(&state_lock);
    uint64_t s = (player_index == 1) ? current_touch_state_2p : current_touch_state_1p;
    LeaveCriticalSection(&state_lock);

    // Pack bits
    // bytes[0]: A1-A5 (bits 0-4)
    state[0] = (s >> 0) & 0x1F;
    // bytes[1]: A6-A8, B1-B2 (bits 5-9)
    state[1] = (s >> 5) & 0x1F;
    // bytes[2]: B3-B7 (bits 10-14)
    state[2] = (s >> 10) & 0x1F;
    // bytes[3]: B8, C1-C2, D1-D2 (bits 15-19)
    state[3] = (s >> 15) & 0x1F;
    // bytes[4]: D3-D7 (bits 20-24)
    state[4] = (s >> 20) & 0x1F;
    // bytes[5]: D8, E1-E4 (bits 25-29)
    state[5] = (s >> 25) & 0x1F;
    // bytes[6]: E5-E8 (bits 30-33)
    state[6] = (s >> 30) & 0x0F;
}
