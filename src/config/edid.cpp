#include "edid.h"

#include <QDir>
#include <QFile>

#include <algorithm>
#include <cstdlib>

namespace riley::config {

namespace {

int byteAt(const QByteArray& b, qsizetype i)
{
    return i >= 0 && i < b.size() ? static_cast<unsigned char>(b[i]) : -1;
}

// Deep colour from the HDMI vendor block of one CTA-861 extension block (128 bytes at `at`), or 0.
int hdmiBits(const QByteArray& edid, qsizetype at)
{
    if (byteAt(edid, at) != 0x02)   // CTA-861 extension tag
        return 0;
    const int dtdStart = byteAt(edid, at + 2);   // the data blocks run from byte 4 up to here
    if (dtdStart < 4)
        return 0;
    int best = 0;
    for (qsizetype i = at + 4; i < at + dtdStart && i < at + 127;) {
        const int head = byteAt(edid, i);
        if (head < 0)
            break;
        const int tag = head >> 5, len = head & 0x1f;
        // The block may stop after the physical address (5 bytes): HDMI with no deep colour.
        if (tag == 3 && len >= 5 && byteAt(edid, i + 1) == 0x03 && byteAt(edid, i + 2) == 0x0c
            && byteAt(edid, i + 3) == 0x00) {
            const int flags = len >= 6 ? byteAt(edid, i + 6) : 0;
            best = 8;
            if (flags & 0x10)
                best = 10;
            if (flags & 0x20)
                best = 12;
            if (flags & 0x40)
                best = 16;
        }
        i += 1 + len;
    }
    return best;
}

}  // namespace

int edidBitsPerColour(const QByteArray& edid)
{
    static const char kHeader[] = {'\x00', '\xff', '\xff', '\xff', '\xff', '\xff', '\xff', '\x00'};
    if (edid.size() < 128 || !edid.startsWith(QByteArray(kHeader, 8)))
        return 0;
    int best = 0;
    const int input = byteAt(edid, 20);
    if (byteAt(edid, 18) == 1 && byteAt(edid, 19) >= 4 && (input & 0x80)) {
        static const int kBits[] = {0, 6, 8, 10, 12, 14, 16, 0};
        best = kBits[(input >> 4) & 7];
    }
    const int extensions = byteAt(edid, 126);
#ifdef RILEY_PLANT_FAULT_BITDEPTH   // the bit-depth gate's planted fault: the HDMI block is not read
    return best;
#endif
    for (int e = 1; e <= extensions && (e + 1) * 128 <= edid.size(); ++e)
        best = std::max(best, hdmiBits(edid, qsizetype(e) * 128));
    return best;
}

QList<int> supportedBitdepths(const QString& connector)
{
    const char* env = std::getenv("RILEY_DRM_DIR");
    const QDir drm(env && *env ? QString::fromLocal8Bit(env) : QStringLiteral("/sys/class/drm"));
    QList<int> out{8};
    if (connector.isEmpty() || connector.contains(QLatin1Char('/')))
        return out;
    for (const QString& card : drm.entryList({QStringLiteral("card*-") + connector}, QDir::Dirs | QDir::System)) {
        if (!card.endsWith(QLatin1Char('-') + connector))
            continue;
        QFile f(drm.filePath(card) + QStringLiteral("/edid"));
        if (f.open(QIODevice::ReadOnly) && edidBitsPerColour(f.readAll()) >= 10) {
            out.append(10);
            break;
        }
    }
    return out;
}

}  // namespace riley::config
