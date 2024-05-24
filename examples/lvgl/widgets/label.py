import lvgl


# Show line wrap, re-color, line align and text scrolling.
def label_1():
    lvgl.Label(
        long_mode=lvgl.LABEL_LONG_WRAP,  # Break the long lines
        text="Recolor is not supported for v9 now.",
        width=150,  # Set smaller width to make the lines wrap
        text_align=lvgl.TEXT_ALIGN_CENTER,
        align=lvgl.ALIGN_CENTER,
        x=0,
        y=-40,
    )
    lvgl.Label(
        long_mode=lvgl.LABEL_LONG_SCROLL_CIRCULAR,  # Circular scroll
        width=150,
        text="It is a circularly scrolling text. ",
        align=lvgl.ALIGN_CENTER,
        x=0,
        y=40,
    )


# Create a fake text shadow
def label_2():
    # Create a style for the shadow
    style_shadow = lvgl.Style(
        text_opa=lvgl.OPA_30,
        text_color=lvgl.color_black(),
    )

    # Create a label for the shadow first (it's in the background)
    shadow_label = lvgl.Label()
    shadow_label.add_style(style_shadow)

    # Create the main label
    main_label = lvgl.Label()
    main_label.text = "A simple method to create\nshadows on a text.\nIt even works with\n\nnewlines     and spaces."

    # Set the same text for the shadow label
    shadow_label.text = main_label.text

    # Position the main label
    main_label.align_as(lvgl.ALIGN_CENTER, 0, 0)

    # Shift the second label down and to the right by 2 pixel
    shadow_label.align_to(main_label, lvgl.ALIGN_TOP_LEFT, 2, 2)


# Show mixed LTR, RTL and Chinese label
def label_3():
    if hasattr(lvgl.Font, "MONTSERRAT_16"):
        lvgl.Label(
            text="In modern terminology, a microcontroller is similar to a system on a chip (SoC).",
            text_font=lvgl.Font.MONTSERRAT_16,
            width=310,
            align=lvgl._ALIGN_TOP_LEFT,
            x=5,
            y=5,
        )
    else:
        print("missing font MONTSERRAT_16")

    if hasattr(lvgl.Font, "DEJAVU_16_PERSIAN_HEBREW"):
        lvgl.Label(
            text="מעבד, או בשמו המלא יחידת עיבוד מרכזית (באנגלית: CPU - Central Processing Unit).",
            base_dir=lvgl.BASE_DIR_RTL,
            text_font=lvgl.Font.DEJAVU_16_PERSIAN_HEBREW,
            width=310,
            align=lvgl.ALIGN_LEFT_MID,
            x=5,
            y=0,
        )
    else:
        print("missing font DEJAVU_16_PERSIAN_HEBREW")

    if hasattr(lvgl.Font, "SIMSUN_16_CJK"):
        lvgl.Label(
            text="嵌入式系统（Embedded System），\n是一种嵌入机械或电气系统内部、具有专一功能和实时计算性能的计算机系统。",
            text_font=lvgl.Font.SIMSUN_16_CJK,
            width=310,
            align=lvgl.ALIGN_BOTTOM_LEFT,
            x=5,
            y=-5,
        )
    else:
        print("missing font SIMSUN_16_CJK")


# Show customizing the circular scrolling animation of a label with `LV_LABEL_LONG_SCROLL_CIRCULAR`
# long mode.
def label_5():
    animation_template = lvgl.Anim(
        delay=1000,  # Wait 1 second to start the first scroll
        repeat_delay=3000,  # Repeat the scroll 3 seconds after the label scrolls back to the initial position
    )

    # Initialize the label style with the animation template
    label_style = lvgl.Style()
    label_style.anim = animation_template

    label1 = lvgl.Label(
        long_mode=lvgl.LABEL_LONG_SCROLL_CIRCULAR,  # Circular scroll
        width=150,
        text="It is a circularly scrolling text. ",
        align=lvgl.ALIGN_CENTER,
        x=0,
        y=40,
    )
    label1.add_style(label_style)  # Add the style to the label
