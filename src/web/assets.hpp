#pragma once

#include <cstddef>

// Filled in by tools/embed_web.py from web/index.html and web/app.css.
namespace web {

extern const char kIndexHtml[];
extern const size_t kIndexHtmlSize;

extern const char kAppCss[];
extern const size_t kAppCssSize;

} // namespace web
