#pragma once
// Multisampled piano data model (no JUCE dependency, unit-testable).
// File names are read like  A0v1.wav  D#1v12.wav  Fs3v16.flac  (note name + optional velocity layer number).
#include "SampleTools.h"
#include <cctype>
#include <map>
#include <regex>
#include <set>

namespace pcm
{
// Parses a file name (without extension). Returns false when it carries no note name (release / pedal / noise files).
inline bool parsePianoName(const std::string& stem, int& midi, int& layer)
{
    static const std::regex re("(^|[^A-Za-z])([A-Ga-g])([#sSbB]?)(-?[0-9])(?:[ _-]?[vV]([0-9]{1,3}))?([^0-9]|$)");
    std::smatch m;
    if (!std::regex_search(stem, m, re)) return false;
    static const int base[7] = { 9, 11, 0, 2, 4, 5, 7 };               // A B C D E F G
    const int semi = base[std::toupper(m[2].str()[0]) - 'A'];
    int acc = 0;
    if (m[3].length()) { const char c = m[3].str()[0]; acc = (c == '#' || c == 's' || c == 'S') ? 1 : -1; }
    midi = (std::stoi(m[4].str()) + 1) * 12 + semi + acc;               // scientific pitch: A0 = 21, C4 = 60
    layer = m[5].length() ? std::stoi(m[5].str()) : 1;
    return midi >= 0 && midi <= 127;
}

struct PianoSet
{
    std::vector<int> roots;                              // sorted MIDI notes that have samples
    int layers = 0;                                      // velocity layers loaded
    std::vector<std::shared_ptr<const Audio>> a;         // [rootIndex * layers + layer]
    std::string info;
    bool valid() const { return !roots.empty() && layers > 0; }

    // Nearest sampled note (ties go to the lower one, which is then pitched up) and the layer for this velocity.
    std::shared_ptr<const Audio> pick(int note, int vel127, int& root) const
    {
        if (!valid()) return nullptr;
        size_t bi = 0; int bd = 1000;
        for (size_t i = 0; i < roots.size(); ++i) { const int d = std::abs(roots[i] - note); if (d < bd) { bd = d; bi = i; } }
        root = roots[bi];
        const int want = std::max(0, std::min(layers - 1, (std::max(1, std::min(127, vel127)) - 1) * layers / 127));
        for (int d = 0; d < layers; ++d)
            for (int s : { -1, 1 })
            {
                const int li = want + s * d;
                if (li >= 0 && li < layers && a[bi * (size_t) layers + (size_t) li]) return a[bi * (size_t) layers + (size_t) li];
                if (d == 0) break;
            }
        return nullptr;
    }
};
} // namespace pcm
