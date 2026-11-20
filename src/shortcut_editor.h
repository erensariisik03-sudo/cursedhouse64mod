#pragma once
#include "text_buffer.h"
#include <string>
namespace shortcut_editor {
enum class Kind { None, Size, Color };
class Editor {
    Kind kind_ = Kind::None;
    std::string before_, value_, tag_;
    int before_cursor_ = 0, before_anchor_ = 0, start_ = 0;
    bool Update(char* text, int& cursor, int& anchor, const std::string& tag) {
        std::string candidate(text);
        if (start_ < 0 || static_cast<size_t>(start_) > candidate.size() || candidate.compare(static_cast<size_t>(start_), tag_.size(), tag_) != 0) return false;
        candidate.replace(static_cast<size_t>(start_), tag_.size(), tag);
        if (text_buffer::Count(candidate.c_str()) > text_buffer::kLimit) return false;
        std::memcpy(text, candidate.c_str(), candidate.size() + 1);
        tag_ = tag;
        cursor = anchor = start_ + static_cast<int>(tag.size());
        return true;
    }
public:
    Kind Current() const { return kind_; }
    const char* Before() const { return before_.c_str(); }
    bool Active() const { return kind_ != Kind::None; }
    bool HasSize() const { return kind_ == Kind::Size && !value_.empty(); }
    void Drop() { kind_ = Kind::None; before_.clear(); value_.clear(); tag_.clear(); }
    void Cancel(char* text, int& cursor, int& anchor) {
        if (!Active()) return;
        std::memcpy(text, before_.c_str(), before_.size() + 1);
        cursor = before_cursor_; anchor = before_anchor_; Drop();
    }
    bool Begin(Kind kind, char* text, int& cursor, int& anchor) {
        if (Active()) Cancel(text, cursor, anchor);
        const int lo = std::min(cursor, anchor), hi = std::max(cursor, anchor);
        const std::string tag = kind == Kind::Size ? "<size=xxx>" : "<xxxx>";
        const std::string candidate = std::string(text).substr(0, lo) + tag + std::string(text).substr(hi);
        // A chosen TMP color has '#' plus four RGBA digits. Reserve that extra byte now.
        if (text_buffer::Count(candidate.c_str()) + (kind == Kind::Color ? 1 : 0) > text_buffer::kLimit) return false;
        before_ = text; before_cursor_ = cursor; before_anchor_ = anchor;
        start_ = lo; tag_ = tag; value_.clear(); kind_ = kind;
        text_buffer::Replace(text, cursor, anchor, tag.c_str());
        cursor = start_ + (kind == Kind::Size ? 6 : 1);
        anchor = cursor + (kind == Kind::Size ? 3 : 4);
        return true;
    }
    bool SizeDigit(char digit, char* text, int& cursor, int& anchor) {
        if (kind_ != Kind::Size || digit < '0' || digit > '9') return false;
        const std::string next = value_ + digit;
        if (!Update(text, cursor, anchor, "<size=" + next + ">")) return false;
        value_ = next; cursor = anchor = start_ + 6 + static_cast<int>(value_.size()); return true;
    }
    bool SizeBackspace(char* text, int& cursor, int& anchor) {
        if (!HasSize()) return false;
        const std::string next = value_.substr(0, value_.size() - 1);
        if (!Update(text, cursor, anchor, "<size=" + (next.empty() ? "xxx" : next) + ">")) return false;
        value_ = next; cursor = anchor = start_ + 6 + static_cast<int>(value_.size()); return true;
    }
    bool CommitSize(int& cursor, int& anchor) {
        if (!HasSize()) return false;
        cursor = anchor = start_ + static_cast<int>(tag_.size()); Drop(); return true;
    }
    bool PickColor(const char* rgba, char* text, int& cursor, int& anchor) {
        if (kind_ != Kind::Color || !rgba || std::strlen(rgba) != 4) return false;
        for (int i = 0; i < 4; ++i) if (!((rgba[i] >= '0' && rgba[i] <= '9') || (rgba[i] >= 'A' && rgba[i] <= 'F'))) return false;
        if (!Update(text, cursor, anchor, std::string("<#") + rgba + ">")) return false;
        Drop(); return true;
    }
};
}
