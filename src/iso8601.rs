use iso8601;

#[derive(Debug, PartialEq)]
pub struct Timezone {
    pub utc: bool,
    pub local: bool,
    pub offset_minutes: i32,
}

#[derive(Debug, PartialEq)]
pub struct DateTime {
    pub year: i32,
    pub month: i32,
    pub day: i32,
    pub week: i32,
    pub weekday: i32,
    pub year_day: i32,
    pub hour: i32,
    pub minute: i32,
    pub second: i32,
    pub sub_second: f64,
    pub tz: Timezone,
}

impl Default for DateTime {
    fn default() -> Self {
        DateTime {
            year: 0,
            month: -1,
            day: -1,
            week: -1,
            weekday: -1,
            year_day: -1,
            hour: -1,
            minute: -1,
            second: -1,
            sub_second: 0.0,
            tz: Timezone {
                utc: false,
                local: false,
                offset_minutes: 0,
            },
        }
    }
}

pub fn parse(s: &str) -> Option<DateTime> {
    if let Ok(dt) = iso8601::datetime(s) {
        let mut res = DateTime::default();
        match dt.date {
            iso8601::Date::YMD { year, month, day } => {
                res.year = year;
                res.month = month as i32;
                res.day = day as i32;
            }
            iso8601::Date::Week { year, ww, d } => {
                res.year = year;
                res.week = ww as i32;
                res.weekday = d as i32;
            }
            iso8601::Date::Ordinal { year, ddd } => {
                res.year = year;
                res.year_day = ddd as i32;
            }
        }

        res.hour = dt.time.hour as i32;
        res.minute = dt.time.minute as i32;
        res.second = dt.time.second as i32;
        res.sub_second = dt.time.millisecond as f64 / 1000.0;

        if dt.time.tz_offset_hours == 0 && dt.time.tz_offset_minutes == 0 {
            res.tz.utc = true;
        } else {
            res.tz.offset_minutes = dt.time.tz_offset_hours * 60 + dt.time.tz_offset_minutes;
        }

        return Some(res);
    }

    if let Ok(d) = iso8601::date(s) {
        let mut res = DateTime::default();
        match d {
            iso8601::Date::YMD { year, month, day } => {
                res.year = year;
                res.month = month as i32;
                res.day = day as i32;
            }
            iso8601::Date::Week { year, ww, d } => {
                res.year = year;
                res.week = ww as i32;
                res.weekday = d as i32;
            }
            iso8601::Date::Ordinal { year, ddd } => {
                res.year = year;
                res.year_day = ddd as i32;
            }
        }
        res.tz.local = true;
        return Some(res);
    }

    None
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_datetime() {
        let res = parse("2024-03-15T14:30:05Z").unwrap();
        assert_eq!(res.year, 2024);
        assert_eq!(res.month, 3);
        assert_eq!(res.day, 15);
        assert_eq!(res.hour, 14);
        assert_eq!(res.minute, 30);
        assert_eq!(res.second, 5);
        assert!(res.tz.utc);
    }

    #[test]
    fn test_parse_date_only() {
        let res = parse("2024-03-15").unwrap();
        assert_eq!(res.year, 2024);
        assert_eq!(res.month, 3);
        assert_eq!(res.day, 15);
        assert_eq!(res.hour, -1);
        assert!(res.tz.local);
    }
}
