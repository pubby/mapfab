#ifndef GRAPHICS_HPP
#define GRAPHICS_HPP

#include <wx/graphics.h>

#ifdef __WXGTK__
  #define DC_RENDER 1
  using render_t = wxDC;
  using bitmap_t = wxBitmap;
#else
  #define GC_RENDER 1
  using render_t = wxGraphicsContext;
  using bitmap_t = wxGraphicsBitmap;
#endif

inline wxGraphicsRenderer* get_renderer()
{
#ifdef __WXMSW__
    return wxGraphicsRenderer::GetDirect2DRenderer();
#else
    return wxGraphicsRenderer::GetDefaultRenderer();
#endif
}

inline void draw_line(render_t& gc, float x0, float y0, float x1, float y1)
{
#ifdef GC_RENDER
    wxPoint2DDouble pts[2] = {{x0, y0}, {x1, y1}};
    gc.DrawLines(2, pts);
#else
    gc.DrawLine(x0, y0, x1, y1);
#endif
}

inline void draw_point(render_t& gc, float x0, float y0)
{
#ifdef GC_RENDER
    wxPoint2DDouble pts[2] = {{x0, y0}, {x0, y0}};
    gc.DrawLines(2, pts);
#else
    gc.DrawPoint(x0, y0);
#endif
}

inline void draw_circle(render_t& gc, float x, float y, float radius)
{
#if GC_RENDER
    gc.DrawEllipse(x - radius, y - radius, radius * 2.0f, radius * 2.0f);
#else
    gc.DrawCircle(x, y, radius);
#endif
}

inline void set_font(render_t& gc, wxFont const& font, wxColour const& color)
{
#if GC_RENDER
    gc.SetFont(font, color);
#else
    gc.SetTextForeground(color);
    gc.SetFont(font);
#endif
}

inline void text_extent(render_t& gc, wxString const& str, int* x, int* y)
{
#if GC_RENDER
    wxDouble xf, yf;
    gc.GetTextExtent(str, &xf, &yf);
    *x = xf;
    *y = yf;
#else
    gc.GetTextExtent(str, x, y);
#endif
}

#endif
