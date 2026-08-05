//! Dead-reckoning feel relative to the last cloth-map sextant reading.
//!
//! Humans walk with a sense of "about fifty steps east of where I checked the
//! map," then reset that feel when they look at the sextant again. The sidecar
//! mirrors that: keep the last fix, accumulate net tile steps across situation
//! flushes, and phrase the offset for Spectron. Not GIS — a soft mental anchor.

use crate::events::SextantFix;

/// Running offset from the last sextant glance (engine tiles, east/north).
#[derive(Debug, Clone, Default)]
pub struct SextantFeel {
    pub fix: Option<SextantFix>,
    /// Positive = east of the fix.
    pub east: i32,
    /// Positive = north of the fix.
    pub north: i32,
}

impl SextantFeel {
    /// New map glance: remember the fix and stand on it (feel resets).
    pub fn reset_to_fix(&mut self, fix: SextantFix) {
        self.fix = Some(fix);
        self.east = 0;
        self.north = 0;
    }

    /// Moongate / jail teleport / etc. — the old feel no longer applies.
    pub fn clear(&mut self) {
        self.fix = None;
        self.east = 0;
        self.north = 0;
    }

    /// Fold one situation stretch's net move into the feel (when a fix is held).
    pub fn apply_stretch(&mut self, heading: &str, net_tiles: u32) {
        if self.fix.is_none() || net_tiles == 0 {
            return;
        }
        if let Some((de, dn)) = displacement_for_heading(heading, net_tiles) {
            self.east += de;
            self.north += dn;
        }
    }

    /// Avatar-facing line for the remember body, or `None` if no fix yet.
    pub fn describe(&self) -> Option<String> {
        let fix = self.fix.as_ref()?;
        let fix_label = format!(
            "{}°{}, {}°{}",
            fix.latitude, fix.latitude_hemi, fix.longitude, fix.longitude_hemi
        );
        if self.east == 0 && self.north == 0 {
            return Some(format!(
                "Feel since last sextant: still at the last map reading ({fix_label})."
            ));
        }
        let (steps, heading) = offset_to_steps_heading(self.east, self.north);
        Some(format!(
            "Feel since last sextant: about {steps} steps {heading} of the last map \
             reading ({fix_label})."
        ))
    }
}

/// Map a stretch heading + chebyshev net tiles onto (east, north) deltas.
fn displacement_for_heading(heading: &str, net_tiles: u32) -> Option<(i32, i32)> {
    let d = net_tiles as i32;
    if d == 0 {
        return None;
    }
    let key = heading.trim();
    if key.is_empty()
        || key.eq_ignore_ascii_case("none")
        || key.eq_ignore_ascii_case("circling")
        || key.eq_ignore_ascii_case("sudden")
        || key.eq_ignore_ascii_case("unknown")
        || key.eq_ignore_ascii_case("teleport")
    {
        return None;
    }
    // Unit (east, north) with chebyshev length ≥ 1; scale so max(|e|,|n|) == d.
    let (ue, un): (i32, i32) = match key.to_ascii_uppercase().as_str() {
        "N" | "NORTH" => (0, 1),
        "NNE" => (1, 2),
        "NE" | "NORTHEAST" => (1, 1),
        "ENE" => (2, 1),
        "E" | "EAST" => (1, 0),
        "ESE" => (2, -1),
        "SE" | "SOUTHEAST" => (1, -1),
        "SSE" => (1, -2),
        "S" | "SOUTH" => (0, -1),
        "SSW" => (-1, -2),
        "SW" | "SOUTHWEST" => (-1, -1),
        "WSW" => (-2, -1),
        "W" | "WEST" => (-1, 0),
        "WNW" => (-2, 1),
        "NW" | "NORTHWEST" => (-1, 1),
        "NNW" => (-1, 2),
        _ => return None,
    };
    let scale = d / ue.abs().max(un.abs()).max(1);
    Some((ue * scale, un * scale))
}

fn offset_to_steps_heading(east: i32, north: i32) -> (u32, &'static str) {
    let steps = east.unsigned_abs().max(north.unsigned_abs());
    // Pick the 16-way label whose unit best matches the offset quadrant.
    use std::cmp::Ordering::{Equal, Greater, Less};
    let heading = match (east.cmp(&0), north.cmp(&0)) {
        (Equal, Greater) => "N",
        (Equal, Less) => "S",
        (Greater, Equal) => "E",
        (Less, Equal) => "W",
        (Greater, Greater) => {
            if north >= 2 * east {
                "NNE"
            } else if east >= 2 * north {
                "ENE"
            } else {
                "NE"
            }
        }
        (Greater, Less) => {
            let sn = (-north).max(0);
            if sn >= 2 * east {
                "SSE"
            } else if east >= 2 * sn {
                "ESE"
            } else {
                "SE"
            }
        }
        (Less, Less) => {
            let we = (-east).max(0);
            let sn = (-north).max(0);
            if sn >= 2 * we {
                "SSW"
            } else if we >= 2 * sn {
                "WSW"
            } else {
                "SW"
            }
        }
        (Less, Greater) => {
            let we = (-east).max(0);
            if north >= 2 * we {
                "NNW"
            } else if we >= 2 * north {
                "WNW"
            } else {
                "NW"
            }
        }
        _ => "N",
    };
    (steps, heading)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn fix() -> SextantFix {
        SextantFix {
            latitude: 30,
            latitude_hemi: "S".into(),
            longitude: 60,
            longitude_hemi: "E".into(),
        }
    }

    #[test]
    fn east_then_west_nets_remaining_east() {
        let mut feel = SextantFeel::default();
        feel.reset_to_fix(fix());
        feel.apply_stretch("E", 50);
        assert_eq!((feel.east, feel.north), (50, 0));
        let d1 = feel.describe().unwrap();
        assert!(d1.contains("50 steps E"), "{d1}");
        assert!(d1.contains("30°S, 60°E"), "{d1}");

        feel.apply_stretch("W", 40);
        assert_eq!((feel.east, feel.north), (10, 0));
        let d2 = feel.describe().unwrap();
        assert!(d2.contains("10 steps E"), "{d2}");
    }

    #[test]
    fn sextant_resets_feel() {
        let mut feel = SextantFeel::default();
        feel.reset_to_fix(fix());
        feel.apply_stretch("N", 20);
        feel.reset_to_fix(SextantFix {
            latitude: 20,
            latitude_hemi: "S".into(),
            longitude: 55,
            longitude_hemi: "E".into(),
        });
        assert_eq!((feel.east, feel.north), (0, 0));
        let d = feel.describe().unwrap();
        assert!(d.contains("still at the last map reading"), "{d}");
        assert!(d.contains("20°S, 55°E"), "{d}");
    }

    #[test]
    fn no_fix_means_no_feel_line() {
        let mut feel = SextantFeel::default();
        feel.apply_stretch("E", 50);
        assert!(feel.describe().is_none());
        assert_eq!(feel.east, 0);
    }

    #[test]
    fn relocate_clears() {
        let mut feel = SextantFeel::default();
        feel.reset_to_fix(fix());
        feel.apply_stretch("S", 12);
        feel.clear();
        assert!(feel.describe().is_none());
    }

    #[test]
    fn diagonal_chebyshev() {
        let mut feel = SextantFeel::default();
        feel.reset_to_fix(fix());
        feel.apply_stretch("NE", 15);
        assert_eq!((feel.east, feel.north), (15, 15));
        let d = feel.describe().unwrap();
        assert!(d.contains("15 steps NE"), "{d}");
    }
}
