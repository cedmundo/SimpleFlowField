#include <stddef.h>
#include <raylib.h>
#include <raymath.h>
#include <limits.h>
#include <memory.h>
#include <stdlib.h>

#define DEMO_ROWS 20
#define DEMO_COLS 20
#define DEMO_SIZE 40

#define COST_MIN 0
#define COST_MAX 20
#define INTEGR_MIN 0
#define INTEGR_MAX UCHAR_MAX

#define PROCESS_FLOW_FIELD_SECS 0.1f

typedef struct {
    int index;
    struct CellItem *next;
} CellItem;

typedef struct {
    CellItem *head;
    CellItem *tail;
    int count;
} CellList;

typedef struct {
    unsigned char *costCells;
    unsigned char *integrCells;
    Vector2 *flowCells;
    int target;
    int rows;
    int cols;
    int arraySize;
    int cellSize;
} FlowField;

CellItem *NewCellItem(int index) {
    CellItem *item = MemAlloc(sizeof(CellItem));
    item->index = index;
    return item;
}

CellList *NewCellList() {
    CellList *list = MemAlloc(sizeof(CellList));
    return list;
}

void FreeCellItem(CellItem *item) {
    MemFree(item);
}

void FreeCellList(CellList *list) {
    CellItem *cur = list->head;
    CellItem *tmp = NULL;
    while (cur != NULL) {
        tmp = (CellItem *)cur->next;
        FreeCellItem(cur);
        cur = tmp;
    }

    MemFree(list);
}

void CellListPushBack(CellList *list, CellItem *item) {
    if (list->head == NULL) {
        list->head = item;
    }

    if (list->tail != NULL) {
        list->tail->next = (struct CellItem *)item;
    }

    list->tail = item;
    list->count++;
}

bool CellListNotEmpty(CellList *list) {
    return list->count > 0;
}

CellItem *CellListPopFront(CellList *list) {
    CellItem *front = list->head;
    if (front != NULL) {
        list->head = (CellItem *)front->next;
        list->count--;
    }

    if (list->tail == front) {
        list->tail = NULL;
    }

    return front;
}

FlowField *NewFlowField(int rows, int cols, int cellSize) {
    FlowField *field = MemAlloc(sizeof(FlowField));
    field->rows = rows;
    field->cols = cols;
    field->cellSize = cellSize;
    field->arraySize = rows * cols;
    field->costCells = MemAlloc(sizeof(unsigned char) * field->arraySize);
    field->integrCells = MemAlloc(sizeof(unsigned char) * field->arraySize);
    field->flowCells = MemAlloc(sizeof(Vector2) * field->arraySize);
    field->target = -1;
    return field;
}

void FreeFlowField(FlowField *field) {
    if (field->costCells != NULL) {
        MemFree(field->costCells);
    }

    if (field->integrCells != NULL) {
        MemFree(field->integrCells);
    }

    if (field->flowCells != NULL) {
        MemFree(field->flowCells);
    }

    MemFree(field);
}

int GetFlowFieldDistance(FlowField *field, int from_index, int to_index) {
    int from_i = from_index / field->cols;
    int from_j = from_index % field->rows;
    int to_i = to_index / field->cols;
    int to_j = to_index % field->rows;
    return abs(to_i - from_i) + abs(to_j - from_j);
}

void GetCellNeighbors(FlowField *field, int index, int neighbors[8], int *count) {
    // 1) Set all indexes to -1 (clear syntax)
    memset(neighbors, -1, sizeof(int) * 8);

    int ix = index / field->cols;
    int iy = index % field->rows;
    int found = 0;
    for (int i = (iy == 0 ? 0 : -1); i <= (iy >= field->cols - 1 ? 0 : 1); i++) {
        for (int j = (ix == 0 ? 0 : -1); j <= (ix >= field->rows - 1 ? 0 : 1); j++) {
            int ci =  ((index / field->cols) + j) * field->cols + ((index % field->rows) + i);
            if (i == 0 && j == 0) {
                continue;
            }

            neighbors[found++] = ci;
        }
    }

    *count = found;
}

void UpdateFlowField(FlowField *field) {
    // Skip if target is not valid
    if (field->target < 0 || field->target > field->arraySize) {
        return;
    }

    //////////////////////////////// CALCULATE INTEGRATION FIELD
    CellList *list = NewCellList();

    // 1) Set all cells to max value.
    memset(field->integrCells, INTEGR_MAX, sizeof(unsigned char) * field->arraySize);
    // 2) Set target node value to min.
    field->integrCells[field->target] = INTEGR_MIN;
    // 2.1) Add target node to open list.
    CellListPushBack(list, NewCellItem(field->target));
    // 3) Pop item from list until its empty
    while (CellListNotEmpty(list)) {
        CellItem *center = CellListPopFront(list);

        int center_index = center->index;
        int center_cost = field->costCells[center_index];
        int center_integr = field->integrCells[center_index];
        int neighbors[8] = { 0 };
        int count = 0;
        GetCellNeighbors(field, center_index, neighbors, &count);
        for (int i = 0; i<count; i++) {
            int neighbor_index = neighbors[i];
            int neighbor_cost = field->costCells[neighbor_index];
            if (neighbor_cost == COST_MAX) { // We hit a wall
                continue;
            }

            int neighbor_to_center_dist = GetFlowFieldDistance(field, neighbor_index, center_index);
            int compound_cost = neighbor_to_center_dist + neighbor_cost + center_cost + center_integr;
            if (compound_cost < field->integrCells[neighbor_index]) {
                if (field->integrCells[neighbor_index] == INTEGR_MAX) {
                    CellListPushBack(list, NewCellItem(neighbor_index));
                }
                field->integrCells[neighbor_index] = compound_cost;
            }
        }

        FreeCellItem(center);
    }
    FreeCellList(list);

    //////////////////////////////// CALCULATE FLOW FIELD
    for (int index=0; index<field->arraySize; index++) {
        // Ignore walls
        if (field->integrCells[index] == INTEGR_MAX) {
            field->flowCells[index] = Vector2Zero();
            continue;
        }

        int neighbors[8] = { 0 };
        int count = 0;
        GetCellNeighbors(field, index, neighbors, &count);

        Vector2 cell_total = { 0 };
        for (int i=0;i<count;i++) {
            int neighbor_index = neighbors[i];
            int cell_size = field->cellSize;
            int origin_x = (index / field->cols * cell_size) + (cell_size / 2);
            int origin_y = (index % field->rows * cell_size) + (cell_size / 2);
            int target_x = (neighbors[i] / field->cols * cell_size) + (cell_size / 2);
            int target_y = (neighbors[i] % field->rows * cell_size) + (cell_size / 2);

            Vector2 origin_point = { (float)origin_x, (float)origin_y };
            Vector2 target_point = {  (float)target_x, (float)target_y };
            Vector2 neighbor_vec = Vector2Scale(Vector2Subtract(origin_point, target_point), field->integrCells[neighbor_index]);
            cell_total = Vector2Add(cell_total, neighbor_vec);
        }
        field->flowCells[index] = Vector2Normalize(cell_total);
    }
}

void DrawFlowField(FlowField *field, int channel) {
    for (int index=0; index<field->arraySize; index++) {
        int i = index / field->cols;
        int j = index % field->rows;
        int cell_size = field->cellSize;

        if (channel == 0) {
            int v = field->costCells[index];
            Color fillColor = ColorFromHSV(320.0f, 0.4f, Remap((float)v, COST_MIN, COST_MAX, 1.0f, 0.0f));
            DrawRectangle(i * cell_size, j * cell_size, cell_size, cell_size, fillColor);
            DrawText(TextFormat("%d", v), i * cell_size + 10, j * cell_size + 10, 10, WHITE);
        } else if (channel == 1 || channel == 2) {
            int v = field->integrCells[index];
            Color fillColor = ColorFromHSV(214.0f, 0.8f, Remap((float)v, INTEGR_MIN, INTEGR_MAX, 1.0f, 0.0f));
            DrawRectangle(i * cell_size, j * cell_size, cell_size, cell_size, fillColor);
            if (channel == 1) {
                DrawText(TextFormat("%d", v), i * cell_size + 10, j * cell_size + 10, 10, WHITE);
            } else {
                int offset_x = (index / field->cols * cell_size) + (cell_size / 2);
                int offset_y = (index % field->rows * cell_size) + (cell_size / 2);

                if (Vector2LengthSqr(field->flowCells[index]) != 0.0f) {
                    Vector2 dir = Vector2Scale(field->flowCells[index], 10.0f);
                    int dir_x = (int)dir.x;
                    int dir_y = (int)dir.y;
                    DrawRectangle(offset_x + dir_x - 2, offset_y + dir_y - 2, 4, 4, WHITE);
                    DrawLine(offset_x, offset_y, offset_x + dir_x, offset_y + dir_y, WHITE);
                } else {
                    DrawCircle(offset_x, offset_y, 2.0f, WHITE);
                }
            }
        }
    }
}

int MapWorldToFlowField(FlowField *field, Vector2 world) {
    Vector2 scaled = Vector2Scale(world, 1.0f / (float) field->cellSize);
    return field->cols * (int)scaled.x + (int)scaled.y;
}

int main() {
    InitWindow(DEMO_COLS * DEMO_SIZE, DEMO_ROWS * DEMO_SIZE, "SimpleFlowField");
    SetWindowMonitor(0);
    SetTargetFPS(60);

    FlowField *field = NewFlowField(DEMO_ROWS, DEMO_COLS, DEMO_SIZE);

    int fieldDrawingChannel = 2;
    unsigned char intensity = COST_MAX;
    float process_flow_field_timer = 0.0f;
    while (!WindowShouldClose()) {
        BeginDrawing();
        {
            ClearBackground(BLACK);
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                int index = MapWorldToFlowField(field, GetMousePosition());
                field->target = index;
            }

            if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
                int index = MapWorldToFlowField(field, GetMousePosition());
                field->costCells[index] = intensity;
            }

            if (IsKeyPressed(KEY_KP_ADD) || IsKeyPressed(KEY_KP_SUBTRACT)) {
                intensity = (unsigned char)Clamp((float)intensity + (IsKeyPressed(KEY_KP_ADD) ? 2.0f : -2.0f), COST_MIN, COST_MAX);
            }

            if (IsKeyDown(KEY_F1)) {
                fieldDrawingChannel = 0;
            } else if (IsKeyDown(KEY_F2)) {
                fieldDrawingChannel = 1;
            } else if (IsKeyDown(KEY_F3)) {
                fieldDrawingChannel = 2;
            }

            process_flow_field_timer += GetFrameTime();
            if (process_flow_field_timer >= PROCESS_FLOW_FIELD_SECS) {
                process_flow_field_timer = 0.0f;
                UpdateFlowField(field);
            }

            DrawFlowField(field, fieldDrawingChannel);
            DrawRectangle(0, 0, 80, 20, WHITE);
            DrawFPS(0, 0);
        }
        EndDrawing();
    }
    FreeFlowField(field);

    CloseWindow();
    return 0;
}
