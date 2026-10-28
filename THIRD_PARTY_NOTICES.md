# Third-party notices

Files in this repository from other projects:

- **Oswald** (`fonts/Oswald-*.ttf`, embedded in each launcher's executable: the titles): https://github.com/googlefonts/OswaldFont,
  Copyright 2016 The Oswald Project Authors, SIL Open Font License 1.1 (`fonts/OFL-Oswald.txt`). Static instances
  (weights 500, 600, 700) of Google Fonts' variable font, made with fontTools' instancer.
- **Source Sans 3** (`fonts/SourceSans3-*.ttf`, embedded likewise: the text): https://github.com/adobe-fonts/source-sans,
  Copyright 2010-2020 Adobe, with Reserved Font Name 'Source', SIL Open Font License 1.1 (`fonts/OFL-SourceSans3.txt`).
  Adobe's own TTF files of release 3.052R (weights 400, 500, 600, 700), unmodified (`SourceSans3-Semibold.ttf` saved
  as `SourceSans3-SemiBold.ttf`).
- **nanosvg** (`thirdparty/nanosvg/`): https://github.com/memononen/nanosvg, Copyright (c) 2013-14 Mikko Mononen,
  zlib license (`thirdparty/nanosvg/LICENSE.txt`). It draws the launcher's icons and art.

The launcher is also built with SDL 3 (zlib), Dear ImGui (MIT), toml++ (MIT) and stb (MIT or public domain), taken
from the ReXGlue SDK's tree; their notices are the SDK's.
