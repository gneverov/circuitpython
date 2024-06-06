import lvgl


def image_1():
    from .. import get_asset

    img1 = lvgl.Image(
        src=get_asset("img_cogwheel_argb.bin"),
        align=lvgl.ALIGN_CENTER,
        scale_x=512,
        scale_y=128,
        rotation=10,
    )

    img2 = lvgl.Image(src="\xEF\x80\x8C" "Accept")
    img2.align_to(img1, lvgl.ALIGN_OUT_BOTTOM_MID, 0, 20)

    img1.bg_opa = 100


# Demonstrate runtime image re-coloring
def image_2():
    def slider_event_cb(e):
        # Recolor the image based on the sliders' values
        color = lvgl.color_make(red_slider.value, green_slider.value, blue_slider.value)
        intense = intense_slider.value
        img1.image_recolor_opa = intense
        img1.image_recolor = color

    def create_slider(color):
        slider = lvgl.Slider(
            min_value=0,
            max_value=255,
            width=10,
            height=200,
        )
        slider[lvgl.PART_KNOB].bg_color = color
        slider[lvgl.PART_INDICATOR].bg_color = lvgl.color_darken(color, lvgl.OPA_40)
        slider.add_event_cb(slider_event_cb, lvgl.EVENT_VALUE_CHANGED)
        return slider

    # Create 4 sliders to adjust RGB color and re-color intensity
    red_slider = create_slider(lvgl.Palette.RED.main())
    green_slider = create_slider(lvgl.Palette.GREEN.main())
    blue_slider = create_slider(lvgl.Palette.BLUE.main())
    intense_slider = create_slider(lvgl.Palette.GREY.main())

    red_slider.value = lvgl.OPA_20
    green_slider.value = lvgl.OPA_90
    blue_slider.value = lvgl.OPA_60
    intense_slider.value = lvgl.OPA_50

    red_slider.align_as(lvgl.ALIGN_LEFT_MID, 25, 0)
    green_slider.align_to(red_slider, lvgl.ALIGN_OUT_RIGHT_MID, 25, 0)
    blue_slider.align_to(green_slider, lvgl.ALIGN_OUT_RIGHT_MID, 25, 0)
    intense_slider.align_to(blue_slider, lvgl.ALIGN_OUT_RIGHT_MID, 25, 0)

    # Now create the actual image
    from .. import get_asset

    img1 = lvgl.Image(src=get_asset("img_cogwheel_argb.bin"))
    img1.align_as(lvgl.ALIGN_RIGHT_MID, -20, 0)

    intense_slider.send_event(lvgl.EVENT_VALUE_CHANGED)


# Show transformations (zoom and rotation) using a pivot point.
def image_3():
    def set_angle(a, v):
        img.rotation = v

    def set_scale(a, v):
        img.scale_x = v
        img.scale_y = v

    # Now create the actual image
    from .. import get_asset

    img = lvgl.Image(src=get_asset("img_cogwheel_argb.bin"))
    img.align_as(lvgl.ALIGN_CENTER, 50, 50)
    img.pivot = (0, 0)  # Rotate around the top left corner

    a = lvgl.Anim(
        var=img,
        duration=5000,
        repeat_count=lvgl.ANIM_REPEAT_INFINITE,
    )
    lvgl.Anim(
        a,
        exec_cb=set_angle,
        start_value=0,
        end_value=3600,
    ).start()
    lvgl.Anim(
        a,
        exec_cb=set_scale,
        start_value=128,
        end_value=256,
        playback_duration=3000,
    ).start()


# Image styling and offset
def image_4():
    def ofs_y_anim(a, v):
        img.offset_y = v

    style = lvgl.Style(
        bg_color=lvgl.Palette.YELLOW.main(),
        bg_opa=lvgl.OPA_COVER,
        image_recolor_opa=lvgl.OPA_COVER,
        image_recolor=lvgl.color_black(),
    )

    img = lvgl.Image()
    img.add_style(style)
    from .. import get_asset

    img.src = get_asset("img_skew_strip.bin")
    img.update(
        width=150,
        height=100,
        align=lvgl.ALIGN_CENTER,
    )

    lvgl.Anim(
        var=img,
        exec_cb=ofs_y_anim,
        start_value=0,
        end_value=100,
        duration=3000,
        playback_duration=500,
        repeat_count=lvgl.ANIM_REPEAT_INFINITE,
    ).start()
