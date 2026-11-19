#include "Splitter.h"

bool Splitter::HandlePointerDown(dui::PointerEvent& ev)
{
    dragging_ = true;
    startX_ = ev.pos.x;
    SetCapture();
    if (onDragStart_) onDragStart_();
    return true;
}

bool Splitter::HandlePointerMove(dui::PointerEvent& ev)
{
    if (!dragging_) return false;
    if (onDrag_) onDrag_(ev.pos.x - startX_);
    return true;
}

bool Splitter::HandlePointerUp(dui::PointerEvent&)
{
    if (!dragging_) return false;
    dragging_ = false;
    if (onDragEnd_) onDragEnd_();
    return true;
}
