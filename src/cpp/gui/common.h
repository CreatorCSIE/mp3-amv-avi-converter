#pragma once
#include <QFileInfo>
#include <QString>
#include <QStringList>

// VIDEO_EXT（main_window.py）
inline const QStringList& videoExts() {
    static const QStringList v = {".mp4", ".avi", ".mkv", ".mp3", ".mov",
                                  ".wmv", ".flv", ".webm", ".ts", ".wav"};
    return v;
}

inline bool isMediaFile(const QString& path) {
    QFileInfo fi(path);
    return fi.isFile() && videoExts().contains(fi.suffix().toLower(), Qt::CaseInsensitive);
}
