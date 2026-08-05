//! Ultima VII-inspired colours and fonts for the sidecar UI.
//! Softer mid-tones like an interior scene: stone, wood, deep blue rugs,
//! parchment for reading — colourful but easy on the eyes.
//!
//! Font: Fondamento (Google Fonts / OFL) — flowing scribe script for titles and body.

use eframe::egui::{
    self, Color32, FontData, FontDefinitions, FontFamily, FontId, RichText, Stroke, Visuals,
};

pub const FONT_DISPLAY: &str = "u7_display";
pub const FONT_BODY: &str = "u7_body";

/// Soft aged gold (accent / headings), not neon speech-yellow.
pub const GOLD: Color32 = Color32::from_rgb(196, 162, 88);
pub const GOLD_DIM: Color32 = Color32::from_rgb(158, 132, 78);
pub const GOLD_BRIGHT: Color32 = Color32::from_rgb(220, 190, 120);

/// Warm brown ink for body text on parchment.
pub const INK: Color32 = Color32::from_rgb(62, 44, 30);

/// Soft shadow under headings (lighter than true black).
pub const SHADOW: Color32 = Color32::from_rgb(48, 40, 34);

/// Mid-tone stone / cobble (U7 walls & floors).
pub const STONE_DEEP: Color32 = Color32::from_rgb(78, 72, 64);
pub const STONE: Color32 = Color32::from_rgb(98, 90, 80);
pub const STONE_LIGHT: Color32 = Color32::from_rgb(118, 108, 96);

/// Warm furniture wood.
pub const WOOD: Color32 = Color32::from_rgb(112, 78, 48);
pub const WOOD_LIGHT: Color32 = Color32::from_rgb(138, 100, 62);

/// Scroll / desk parchment for reading panels.
pub const PARCHMENT: Color32 = Color32::from_rgb(214, 198, 162);

/// Deep blue tapestry / rug accents.
pub const RUG_BLUE: Color32 = Color32::from_rgb(48, 72, 128);
pub const RUG_BLUE_LIGHT: Color32 = Color32::from_rgb(72, 98, 158);

/// Muted grass green (sparingly).
pub const MOSS: Color32 = Color32::from_rgb(72, 108, 64);

/// Soft crimson cushion accent.
pub const CREST: Color32 = Color32::from_rgb(148, 58, 58);

fn family_display() -> FontFamily {
    FontFamily::Name(FONT_DISPLAY.into())
}

fn family_body() -> FontFamily {
    FontFamily::Name(FONT_BODY.into())
}

pub fn install_fonts(ctx: &egui::Context) {
    let mut fonts = FontDefinitions::default();
    // One face for titles and body: denser and less jarring than MedievalSharp,
    // still medieval. Arrows / symbols not in the face should use ASCII (e.g. ->).
    fonts.font_data.insert(
        FONT_BODY.to_owned(),
        FontData::from_static(include_bytes!("../assets/fonts/Fondamento-Regular.ttf")).into(),
    );
    fonts.font_data.insert(
        FONT_DISPLAY.to_owned(),
        FontData::from_static(include_bytes!("../assets/fonts/Fondamento-Regular.ttf")).into(),
    );
    fonts
        .families
        .entry(FontFamily::Proportional)
        .or_default()
        .insert(0, FONT_BODY.to_owned());
    fonts
        .families
        .entry(family_display())
        .or_default()
        .insert(0, FONT_DISPLAY.to_owned());
    fonts
        .families
        .entry(family_body())
        .or_default()
        .insert(0, FONT_BODY.to_owned());
    ctx.set_fonts(fonts);
}

pub fn apply_visuals(ctx: &egui::Context) {
    let mut v = Visuals::dark();
    v.dark_mode = true;
    v.override_text_color = Some(Color32::from_rgb(230, 220, 200));

    v.widgets.noninteractive.fg_stroke = Stroke::new(1.0, Color32::from_rgb(220, 210, 190));
    v.widgets.inactive.fg_stroke = Stroke::new(1.0, Color32::from_rgb(235, 225, 205));
    v.widgets.hovered.fg_stroke = Stroke::new(1.0, Color32::from_rgb(250, 240, 220));
    v.widgets.active.fg_stroke = Stroke::new(1.0, Color32::from_rgb(255, 248, 230));
    v.widgets.open.fg_stroke = Stroke::new(1.0, Color32::from_rgb(250, 240, 220));

    v.widgets.noninteractive.bg_fill = STONE;
    v.widgets.inactive.bg_fill = STONE_LIGHT;
    v.widgets.hovered.bg_fill = WOOD_LIGHT;
    v.widgets.active.bg_fill = WOOD;
    v.widgets.open.bg_fill = WOOD_LIGHT;

    v.widgets.noninteractive.bg_stroke = Stroke::new(1.0, Color32::from_rgb(130, 120, 105));
    v.widgets.inactive.bg_stroke = Stroke::new(1.2, Color32::from_rgb(140, 128, 108));
    v.widgets.hovered.bg_stroke = Stroke::new(1.5, GOLD_DIM);
    v.widgets.active.bg_stroke = Stroke::new(1.5, GOLD);

    v.panel_fill = STONE_DEEP;
    v.window_fill = STONE;
    v.extreme_bg_color = Color32::from_rgb(58, 54, 48);
    v.faint_bg_color = Color32::from_rgb(88, 82, 72);
    v.window_stroke = Stroke::new(1.5, RUG_BLUE_LIGHT);
    v.hyperlink_color = RUG_BLUE_LIGHT;
    v.selection.bg_fill = Color32::from_rgb(90, 100, 140);
    v.selection.stroke = Stroke::new(1.0, GOLD_DIM);

    ctx.set_visuals(v);

    let mut style = (*ctx.style()).clone();
    style.text_styles = [
        (
            egui::TextStyle::Heading,
            FontId::new(26.0, family_display()),
        ),
        (egui::TextStyle::Body, FontId::new(17.0, family_body())),
        (egui::TextStyle::Button, FontId::new(16.0, family_body())),
        (egui::TextStyle::Small, FontId::new(14.0, family_body())),
        (
            egui::TextStyle::Monospace,
            FontId::new(13.0, FontFamily::Monospace),
        ),
    ]
    .into();
    style.spacing.item_spacing = egui::vec2(10.0, 8.0);
    style.spacing.button_padding = egui::vec2(14.0, 8.0);
    ctx.set_style(style);
}

pub fn gold_muted(text: impl Into<String>) -> RichText {
    RichText::new(text)
        .color(Color32::from_rgb(210, 198, 170))
        .size(15.0)
        .family(family_body())
}

/// Soft gold heading with a gentle shadow (Fondamento display face).
pub fn paint_shadowed_label(ui: &mut egui::Ui, text: &str, size: f32) {
    let font = FontId::new(size, family_display());
    let galley = ui.fonts(|f| f.layout_no_wrap(text.to_owned(), font.clone(), GOLD_BRIGHT));
    let (rect, _) =
        ui.allocate_exact_size(galley.size() + egui::vec2(2.0, 2.0), egui::Sense::hover());
    let painter = ui.painter();
    let origin = rect.min;
    painter.galley(
        origin + egui::vec2(1.0, 1.0),
        ui.fonts(|f| f.layout_no_wrap(text.to_owned(), font.clone(), SHADOW)),
        SHADOW,
    );
    painter.galley(origin, galley, GOLD_BRIGHT);
}

/// Selectable body copy on parchment (drag to select, Cmd/Ctrl-C to copy).
pub fn paint_shadowed_paragraph(ui: &mut egui::Ui, text: &str, size: f32) {
    ui.add(
        egui::Label::new(
            egui::RichText::new(text)
                .color(INK)
                .size(size)
                .family(family_body()),
        )
        .wrap()
        .selectable(true),
    );
}

pub fn stone_frame() -> egui::Frame {
    egui::Frame::new()
        .fill(PARCHMENT)
        .stroke(Stroke::new(2.0, RUG_BLUE))
        .corner_radius(4.0)
        .inner_margin(egui::Margin::same(14))
        .outer_margin(egui::Margin::same(4))
}

pub fn panel_frame(fill: Color32) -> egui::Frame {
    egui::Frame::new()
        .fill(fill)
        .stroke(Stroke::new(1.5, Color32::from_rgb(120, 110, 95)))
        .inner_margin(egui::Margin::symmetric(14, 10))
}
