#include "../font/ttf.h"

// 功能：声明待测试的贝塞尔曲线 BMP 绘制函数
void draw_bezier_to_bitmap(
    int width,
    int height,
    u8 *color_data,
    u8 color[3],
    point *point_data,
    int count,
    int offset_x,
    int offset_y
);

// 功能：验证字体坐标到 BMP 行列坐标的方向转换
int main(void)
{
    const int width = 10;
    const int height = 10;
    u8 color_data[10 * 10 * 3] = {0};
    u8 color[3] = {0x11, 0x22, 0x33};
    point points[2] = {
        {2.0f, 3.0f, 1},
        {2.0f, 3.0f, 1}
    };
    int row;
    int column;
    int pixel_index;

    // 绘制退化为单点的直线，便于精确检查目标像素
    draw_bezier_to_bitmap(
        width,
        height,
        color_data,
        color,
        points,
        2,
        1,
        2
    );

    /*
     * TTF 坐标中 x 向右、y 向上。
     * BMP 坐标中列向右、行向下。
     */
    column = 2 + 1;
    row = height - 1 - (3 + 2);
    pixel_index = (row * width + column) * 3;

    if (color_data[pixel_index] != color[0] ||
        color_data[pixel_index + 1] != color[1] ||
        color_data[pixel_index + 2] != color[2]) {
        fprintf(stderr, "orientation mapping failed: expected row=%d column=%d\n", row, column);
        return 1;
    }

    return 0;
}
