// The profiles' built-in avatars and colors (the profiles mockup's): each avatar an SVG body in a 100x100 box, drawn
// in a circle; the key is what profile.toml keeps.

#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace launcher::profiles {

struct Avatar {
  const char* key;
  const char* body;  // SVG elements in a viewBox of 0 0 100 100
};

inline constexpr std::array<Avatar, 5> kAvatars{{
    {"fox",
     "<rect width='100' height='100' fill='#2E3B4E'/>"
     "<polygon points='20,40 32,10 46,32' fill='#F08A3C'/><polygon points='54,32 68,10 80,40' fill='#F08A3C'/>"
     "<polygon points='27,34 32,20 39,31' fill='#5A2A12'/><polygon points='61,31 68,20 73,34' fill='#5A2A12'/>"
     "<path d='M18 40 Q50 22 82 40 L66 74 Q50 86 34 74 Z' fill='#F08A3C'/>"
     "<path d='M34 58 Q50 50 66 58 L57 76 Q50 82 43 76 Z' fill='#F4EDE4'/>"
     "<circle cx='38' cy='49' r='4.5' fill='#1A1A1A'/><circle cx='62' cy='49' r='4.5' fill='#1A1A1A'/>"
     "<circle cx='50' cy='70' r='4' fill='#1A1A1A'/>"},
    {"owl",
     "<rect width='100' height='100' fill='#3B2F5C'/>"
     "<polygon points='22,34 28,12 42,28' fill='#8E7BD6'/><polygon points='58,28 72,12 78,34' fill='#8E7BD6'/>"
     "<ellipse cx='50' cy='60' rx='31' ry='33' fill='#8E7BD6'/><ellipse cx='50' cy='74' rx='17' ry='15' fill='#B8AAEB'/>"
     "<circle cx='37' cy='50' r='13' fill='#F4EDE4'/><circle cx='63' cy='50' r='13' fill='#F4EDE4'/>"
     "<circle cx='38' cy='51' r='6' fill='#1A1A1A'/><circle cx='62' cy='51' r='6' fill='#1A1A1A'/>"
     "<polygon points='45,60 55,60 50,70' fill='#F2C14E'/>"},
    {"robot",
     "<rect width='100' height='100' fill='#1F4A47'/>"
     "<rect x='48' y='14' width='4' height='16' fill='#9FE3D6'/><circle cx='50' cy='13' r='6' fill='#F2894B'/>"
     "<rect x='16' y='46' width='8' height='18' rx='3' fill='#6FBFB2'/>"
     "<rect x='76' y='46' width='8' height='18' rx='3' fill='#6FBFB2'/>"
     "<rect x='22' y='30' width='56' height='48' rx='10' fill='#9FE3D6'/>"
     "<rect x='32' y='44' width='13' height='12' rx='3' fill='#13302E'/>"
     "<rect x='55' y='44' width='13' height='12' rx='3' fill='#13302E'/>"
     "<rect x='36' y='64' width='28' height='6' rx='3' fill='#13302E'/>"},
    {"cat",
     "<rect width='100' height='100' fill='#4A2A3A'/>"
     "<polygon points='24,44 28,14 46,32' fill='#F28AB2'/><polygon points='54,32 72,14 76,44' fill='#F28AB2'/>"
     "<polygon points='29,36 31,22 40,31' fill='#B8507A'/><polygon points='60,31 69,22 71,36' fill='#B8507A'/>"
     "<circle cx='50' cy='58' r='29' fill='#F28AB2'/>"
     "<ellipse cx='40' cy='55' rx='4.5' ry='7' fill='#1A1A1A'/><ellipse cx='60' cy='55' rx='4.5' ry='7' fill='#1A1A1A'/>"
     "<polygon points='46,65 54,65 50,70' fill='#7A2B4A'/>"
     "<g stroke='#F4EDE4' stroke-width='2' stroke-linecap='round'><path d='M30 66 L16 63'/><path d='M30 71 L17 73'/>"
     "<path d='M70 66 L84 63'/><path d='M70 71 L83 73'/></g>"},
    {"skull",
     "<rect width='100' height='100' fill='#2A2A2E'/>"
     "<circle cx='50' cy='46' r='27' fill='#EDE6D6'/><rect x='37' y='60' width='26' height='20' rx='5' fill='#EDE6D6'/>"
     "<circle cx='39' cy='47' r='8' fill='#2A2A2E'/><circle cx='61' cy='47' r='8' fill='#2A2A2E'/>"
     "<polygon points='46,58 54,58 50,64' fill='#2A2A2E'/>"
     "<g stroke='#2A2A2E' stroke-width='2.5'><path d='M44 70 V80'/><path d='M50 70 V80'/><path d='M56 70 V80'/></g>"},
}};

// A photo's place before one is chosen, or when its file is missing.
inline constexpr const char* kPhotoPlaceholder =
    "<rect width='100' height='100' fill='#3A4150'/><circle cx='50' cy='40' r='17' fill='#9AA3B2'/>"
    "<path d='M16 100 Q18 64 50 64 Q82 64 84 100 Z' fill='#9AA3B2'/>";
inline constexpr std::string_view kPhoto = "photo";

// The avatar's ring and the profile's banner: turquoise, coral, violet, gold, sky, pink.
inline constexpr std::array<uint32_t, 6> kColors{0x4FC1B5, 0xF07B6B, 0xA48BF0, 0xE8C547, 0x5BB8F2, 0xF28AB2};

inline const Avatar* FindAvatar(std::string_view key) {
  for (const Avatar& avatar : kAvatars) {
    if (key == avatar.key) {
      return &avatar;
    }
  }
  return nullptr;
}

}  // namespace launcher::profiles
