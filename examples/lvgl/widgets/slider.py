import lvgl


# A default slider with a label displaying the current value
def slider_1():
    # Create a slider in the center of the display
    slider = lvgl.Slider(
        align=lvgl.ALIGN_CENTER,
        anim_duration=2000,
    )

    # Create a label below the slider
    slider_label = lvgl.Label(text="0%")
    slider_label.align_to(slider, lvgl.ALIGN_OUT_BOTTOM_MID, 0, 10)

    def slider_event_cb(e):
        slider_label.text = f"{slider.value}%"
        slider_label.align_to(slider, lvgl.ALIGN_OUT_BOTTOM_MID, 0, 10)

    slider.add_event(slider_event_cb, lvgl.EVENT_VALUE_CHANGED)


# Show how to style a slider.
def slider_2():
    # Create a transition
    props = ["bg_color"]
    transition_dsc = lvgl.StyleTransitionDsc(props, lvgl.AnimPath.LINEAR, 300, 0)

    style_main = lvgl.Style(
        bg_opa=lvgl.OPA_COVER,
        bg_color=0xBBB,
        radius=lvgl.RADIUS_CIRCLE,
        pad_top=-2,  # Makes the indicator larger
        pad_bottom=-2,
    )

    style_indicator = lvgl.Style(
        bg_opa=lvgl.OPA_COVER,
        bg_color=lvgl.Palette.CYAN.main(),
        radius=lvgl.RADIUS_CIRCLE,
        transition=transition_dsc,
    )

    style_knob = lvgl.Style(
        bg_opa=lvgl.OPA_COVER,
        bg_color=lvgl.Palette.CYAN.main(),
        border_color=lvgl.Palette.CYAN.darken(3),
        border_width=2,
        radius=lvgl.RADIUS_CIRCLE,
        pad_left=6,
        pad_right=6,
        pad_top=6,
        pad_bottom=6,
        transition=transition_dsc,
    )

    style_pressed_color = lvgl.Style(
        bg_color=lvgl.Palette.CYAN.darken(3),
    )

    # Create a slider and add the style
    slider = lvgl.Slider()
    # lv_obj_remove_style_all(slider);        /*Remove the styles coming from the theme*/

    slider.add_style(style_main, lvgl.PART_MAIN)
    slider.add_style(style_indicator, lvgl.PART_INDICATOR)
    slider.add_style(style_pressed_color, lvgl.PART_INDICATOR | lvgl.STATE_PRESSED)
    slider.add_style(style_knob, lvgl.PART_KNOB)
    slider.add_style(style_pressed_color, lvgl.PART_KNOB | lvgl.STATE_PRESSED)

    slider.align = lvgl.ALIGN_CENTER


# Slider with opposite direction
def slider_4():
    # Create a slider in the center of the display
    slider = lvgl.Slider(
        align=lvgl.ALIGN_CENTER,
        # Reverse the direction of the slider
        min_value=100,
        max_value=0,
    )

    # Create a label below the slider
    slider_label = lvgl.Label(text="0%")
    slider_label.align_to(slider, lvgl.ALIGN_OUT_BOTTOM_MID, 0, 10)

    def slider_event_cb(e):
        slider_label.text = f"{slider.value}%"
        slider_label.align_to(slider, lvgl.ALIGN_OUT_BOTTOM_MID, 0, 10)

    slider.add_event(slider_event_cb, lvgl.EVENT_VALUE_CHANGED)
