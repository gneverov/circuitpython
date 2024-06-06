import lvgl


def line_1():
    # Create an array for the points of the line
    line_points = [(5, 5), (70, 70), (120, 10), (180, 60), (240, 10)]

    # Create style
    style_line = lvgl.Style(
        line_width=8,
        line_color=lvgl.Palette.BLUE.main(),
        line_rounded=True,
    )

    # Create a line and apply the new style
    line1 = lvgl.Line()
    line1.points = line_points  # Set the points
    line1.add_style(style_line)
    line1.align = lvgl.ALIGN_CENTER
