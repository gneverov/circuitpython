import lvgl


def canvas_1():
    CANVAS_WIDTH = 200
    CANVAS_HEIGHT = 150

    rect_dsc = lvgl.draw.RectDsc(
        radius=10,
        bg_opa=lvgl.OPA_COVER,
        # bg_grad = lvgl.GradDsc(dir=lvgl.GRAD_DIR_VER),
        # rect_dsc.bg_grad.dir = LV_GRAD_DIR_VER;
        # rect_dsc.bg_grad.stops[0].color = lv_palette_main(LV_PALETTE_RED);
        # rect_dsc.bg_grad.stops[0].opa = LV_OPA_100;
        # rect_dsc.bg_grad.stops[1].color = lv_palette_main(LV_PALETTE_BLUE);
        # rect_dsc.bg_grad.stops[1].opa = LV_OPA_50;
        border_width=2,
        border_opa=lvgl.OPA_90,
        border_color=lvgl.color_white(),
        shadow_width=5,
        shadow_offset_x=5,
        shadow_offset_y=5,
    )

    label_dsc = lvgl.draw.LabelDsc(
        color=lvgl.Palette.ORANGE.main(),
        text="Some text on text canvas",
    )

    # Create a buffer for the canvas
    draw_buf_16bpp = lvgl.draw.Buffer(CANVAS_WIDTH, CANVAS_HEIGHT, lvgl.COLOR_FORMAT_RGB565)

    canvas = lvgl.Canvas(align=lvgl.ALIGN_CENTER)
    canvas.set_buffer(draw_buf_16bpp)
    canvas.fill_bg(lvgl.Palette.GREY.lighten(3), lvgl.OPA_COVER)

    with canvas.layer() as layer:
        rect_dsc.draw(layer, (30, 20, 100, 70))
        label_dsc.draw(layer, (40, 80, 100, 120))

    # Test the rotation. It requires another buffer where the original image is stored.
    # So use previous canvas as image and rotate it to the new canvas
    draw_buf_32bpp = lvgl.draw.Buffer(
        CANVAS_WIDTH,
        CANVAS_HEIGHT,
        lvgl.COLOR_FORMAT_ARGB8888,
    )
    # Create a canvas and initialize its palette
    canvas2 = lvgl.Canvas(align=lvgl.ALIGN_CENTER)
    canvas2.set_buffer(draw_buf_32bpp)
    canvas2.fill_bg(0xCCC, lvgl.OPA_COVER)

    canvas2.fill_bg(lvgl.Palette.GREY.lighten(1), lvgl.OPA_COVER)

    with canvas2.layer() as layer:
        img_dsc = lvgl.draw.ImageDsc(
            rotation=120,
            src=draw_buf_16bpp,
            pivot_x=CANVAS_WIDTH // 2,
            pivot_y=CANVAS_HEIGHT // 2,
        )
        img_dsc.draw(layer, (0, 0, CANVAS_WIDTH - 1, CANVAS_HEIGHT - 1))


# Create a transparent canvas with transparency
def canvas_2():
    CANVAS_WIDTH = 80
    CANVAS_HEIGHT = 40

    lvgl.screen.bg_color = lvgl.Palette.RED.lighten(5)

    # Create a buffer for the canvas
    draw_buf = lvgl.draw.Buffer(CANVAS_WIDTH, CANVAS_HEIGHT, lvgl.COLOR_FORMAT_ARGB8888)
    # Create a canvas and initialize its palette
    canvas = lvgl.Canvas(align=lvgl.ALIGN_CENTER)
    canvas.set_buffer(draw_buf)

    # Red background (There is no dedicated alpha channel in indexed images so LV_OPA_COVER is ignored)
    blue = lvgl.Palette.BLUE.main()
    canvas.fill_bg(blue)

    # Create hole on the canvas
    for y in range(10, 20):
        for x in range(5, 75):
            canvas.set_px(x, y, blue, lvgl.OPA_50)

    for y in range(20, 30):
        for x in range(5, 75):
            canvas.set_px(x, y, blue, lvgl.OPA_20)

    for y in range(30, 40):
        for x in range(5, 75):
            canvas.set_px(x, y, blue, lvgl.OPA_0)


# Draw a rectangle to the canvas
def canvas_3():
    CANVAS_WIDTH = 50
    CANVAS_HEIGHT = 50

    # Create a buffer for the canvas
    draw_buf = lvgl.draw.Buffer(CANVAS_WIDTH, CANVAS_HEIGHT, lvgl.COLOR_FORMAT_ARGB8888)

    # Create a canvas and initialize its palette
    canvas = lvgl.Canvas(align=lvgl.ALIGN_CENTER)
    canvas.set_buffer(draw_buf)
    canvas.fill_bg(0xCCC)

    dsc = lvgl.draw.RectDsc(
        bg_color=lvgl.Palette.RED.main(),
        border_color=lvgl.Palette.BLUE.main(),
        border_width=3,
        outline_color=lvgl.Palette.GREEN.main(),
        outline_width=2,
        outline_pad=2,
        outline_opa=lvgl.OPA_50,
        radius=5,
    )

    with canvas.layer() as layer:
        dsc.draw(layer, (10, 10, 40, 30))


# Draw a text to the canvas
def canvas_4():
    CANVAS_WIDTH = 50
    CANVAS_HEIGHT = 50

    # Create a buffer for the canvas
    draw_buf = lvgl.draw.Buffer(CANVAS_WIDTH, CANVAS_HEIGHT, lvgl.COLOR_FORMAT_ARGB8888)

    # Create a canvas and initialize its palette
    canvas = lvgl.Canvas(align=lvgl.ALIGN_CENTER)
    canvas.set_buffer(draw_buf)
    canvas.fill_bg(0xCCC)

    dsc = lvgl.draw.LabelDsc(
        color=lvgl.Palette.RED.main(),
        font=lvgl.Font.MONTSERRAT_18,
        decor=lvgl.TEXT_DECOR_UNDERLINE,
        text="Hello",
    )

    with canvas.layer() as layer:
        dsc.draw(layer, (10, 10, 30, 60))


# Draw an arc to the canvas
def canvas_5():
    CANVAS_WIDTH = 50
    CANVAS_HEIGHT = 50

    # Create a buffer for the canvas
    draw_buf = lvgl.draw.Buffer(CANVAS_WIDTH, CANVAS_HEIGHT, lvgl.COLOR_FORMAT_ARGB8888)

    # Create a canvas and initialize its palette
    canvas = lvgl.Canvas(align=lvgl.ALIGN_CENTER)
    canvas.set_buffer(draw_buf)
    canvas.fill_bg(0xCCC)

    dsc = lvgl.draw.ArcDsc(
        color=lvgl.Palette.RED.main(),
        center_x=25,
        center_y=25,
        width=10,
        radius=15,
        start_angle=0,
        end_angle=220,
    )

    with canvas.layer() as layer:
        dsc.draw(layer)


# Draw an image to the canvas
def canvas_6():
    CANVAS_WIDTH = 50
    CANVAS_HEIGHT = 50

    # Create a buffer for the canvas
    draw_buf = lvgl.draw.Buffer(CANVAS_WIDTH, CANVAS_HEIGHT, lvgl.COLOR_FORMAT_ARGB8888)

    # Create a canvas and initialize its palette
    canvas = lvgl.Canvas(align=lvgl.ALIGN_CENTER)
    canvas.set_buffer(draw_buf)
    canvas.fill_bg(0xCCC)

    from .. import get_asset

    header = lvgl.draw.ImageDecoder.get_info(get_asset("img_star.bin"))
    dsc = lvgl.draw.ImageDsc(src=get_asset("img_star.bin"))

    with canvas.layer() as layer:
        dsc.draw(layer, (10, 10, 10 + header.w - 1, 10 + header.h - 1))


# Draw a line to the canvas
def canvas_7():
    CANVAS_WIDTH = 50
    CANVAS_HEIGHT = 50

    # Create a buffer for the canvas
    draw_buf = lvgl.draw.Buffer(CANVAS_WIDTH, CANVAS_HEIGHT, lvgl.COLOR_FORMAT_ARGB8888)

    # Create a canvas and initialize its palette
    canvas = lvgl.Canvas(align=lvgl.ALIGN_CENTER)
    canvas.set_buffer(draw_buf)
    canvas.fill_bg(0xCCC)

    dsc = lvgl.draw.LineDsc(
        color=lvgl.Palette.RED.main(),
        width=4,
        round_end=1,
        round_start=1,
        p1_x=15,
        p1_y=15,
        p2_x=35,
        p2_y=10,
    )

    with canvas.layer() as layer:
        dsc.draw(layer)
