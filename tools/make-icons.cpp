// Renders app/data/icons/justanimate.svg into every packaged icon:
// .ico, .icns, Linux hicolor PNGs and the runtime PNGs.
// Usage: make_icons <repo root>   (cmake --build build --target make_icons)
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>
#include <QtEndian>
#include <cstdio>
#include <cstdlib>

static QByteArray renderPng(QSvgRenderer& svg, int size)
{
    QImage img(size, size, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter p(&img);
    svg.render(&p);
    p.end();
    QByteArray data;
    QBuffer buf(&data);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG");
    return data;
}

static void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size()) {
        std::fprintf(stderr, "cannot write %s\n", qPrintable(path));
        std::exit(1);
    }
}

template<typename T> static void put(QByteArray& a, T v, bool big = false)
{
    T e = big ? qToBigEndian(v) : qToLittleEndian(v);
    a.append(reinterpret_cast<const char*>(&e), sizeof e);
}

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    if (argc < 2) return 2;
    const QString root = QString::fromLocal8Bit(argv[1]) + "/app/data/";
    QSvgRenderer svg(root + "icons/justanimate.svg");
    if (!svg.isValid()) return 1;

    // ICO with PNG entries (Windows Vista and later)
    const int icoSizes[] = { 16, 24, 32, 48, 64, 256 };
    QByteArray head, body;
    put<quint16>(head, 0);
    put<quint16>(head, 1);
    put<quint16>(head, 6);
    quint32 offset = 6 + 16 * 6;
    for (int s : icoSizes) {
        const QByteArray d = renderPng(svg, s);
        const char dim = char(s == 256 ? 0 : s);
        head.append(dim);
        head.append(dim);
        head.append(char(0));
        head.append(char(0));
        put<quint16>(head, 1);
        put<quint16>(head, 32);
        put<quint32>(head, quint32(d.size()));
        put<quint32>(head, offset);
        offset += quint32(d.size());
        body += d;
    }
    writeFile(root + "justanimate.ico", head + body);

    // ICNS: PNG payloads keyed by OSType, each chunk is type + big-endian length + data
    const struct { const char* type; int size; } icns[] = {
        { "icp4", 16 }, { "icp5", 32 }, { "ic11", 32 }, { "icp6", 64 }, { "ic12", 64 },
        { "ic07", 128 }, { "ic13", 256 }, { "ic08", 256 }, { "ic14", 512 }, { "ic09", 512 } };
    QByteArray chunks;
    for (const auto& e : icns) {
        const QByteArray d = renderPng(svg, e.size);
        chunks += e.type;
        put<quint32>(chunks, quint32(d.size() + 8), true);
        chunks += d;
    }
    QByteArray icnsFile("icns");
    put<quint32>(icnsFile, quint32(chunks.size() + 8), true);
    writeFile(root + "justanimate.icns", icnsFile + chunks);

    for (int s : { 16, 24, 32, 48, 64, 128, 256 })
        writeFile(QString("%1hicolor/%2x%2/apps/io.github.makinoatorie.JustAnimate.png").arg(root).arg(s), renderPng(svg, s));
    writeFile(root + "icons/icon.png", renderPng(svg, 256));
    writeFile(root + "icons/logo.png", renderPng(svg, 72));
    return 0;
}
