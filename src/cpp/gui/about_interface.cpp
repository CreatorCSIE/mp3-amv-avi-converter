#include "about_interface.h"

#include <QClipboard>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QProcess>
#include <QScrollArea>
#include <QSysInfo>
#include <QUrl>
#include <QVBoxLayout>
#include <QtGlobal>

#include "fluent/icons.h"
#include "fluent/widgets.h"

namespace {

const char* REPO_URL = "https://github.com/CreatorCSIE/mp3-amv-avi-converter";
const char* APP_VERSION = "0.0.1";

// 许可证均经实查（各上游 LICENSE 原文），不虚构
struct Ref {
    const char* name, *desc, *lic, *url;
};
const std::vector<Ref> REFERENCES = {
    {"FFmpeg", "解封装/缩放/donor 容器（外部进程调用）", "LGPL-3.0 / GPL-2.0（按构建）",
     "https://github.com/FFmpeg/FFmpeg"},
    {"Qt 6 (Widgets/Multimedia)", "GUI 框架", "LGPL-3.0", "https://www.qt.io"},
    {"NumPy", "比特流内核的 PySide6 预言机参照（校验用，不随附）", "BSD-3-Clause",
     "https://github.com/numpy/numpy"},
    {"atj2157-toolkit (zanzipanzi)", "ATJ2157 固件解析与 mmm_vp walker 预言机", "MIT",
     "https://github.com/zanzipanzi/atj2157-toolkit"},
    {"FFmpeg 1.0 (avcodec-54)", "原版工具内置编码器——比特流行为对照基准（仅观察，不随附）",
     "LGPL-2.0+", "https://github.com/FFmpeg/FFmpeg"},
};

QString ffmpegVersionLine() {
    QProcess ff;
    ff.start("ffmpeg", {"-version"});
    if (!ff.waitForFinished(10000)) return "未找到";
    const QString out = QString::fromUtf8(ff.readAllStandardOutput());
    const QStringList lines = out.split('\n');
    return lines.isEmpty() || lines[0].trimmed().isEmpty() ? "未找到" : lines[0].trimmed();
}

}  // namespace

FoldGroup::FoldGroup(const QString& title, QWidget* parent) : QFrame(parent) {
    setObjectName("card");
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(12, 6, 12, 10);
    lay->setSpacing(6);
    head_ = new TransparentPushButton(FluentIcon::DOWN, title, this);
    body_ = new QWidget(this);
    bodyLay_ = new QVBoxLayout(body_);
    bodyLay_->setContentsMargins(4, 0, 4, 0);
    bodyLay_->setSpacing(8);
    body_->setVisible(false);
    connect(head_, &QPushButton::clicked, this, &FoldGroup::toggle);
    lay->addWidget(head_);
    lay->addWidget(body_);
}

void FoldGroup::toggle() {
    open_ = !open_;
    body_->setVisible(open_);
    head_->setIcon(fluentIcon(open_ ? FluentIcon::UP : FluentIcon::DOWN));
}

void FoldGroup::addWidget(QWidget* w, int stretch, Qt::Alignment align) {
    bodyLay_->addWidget(w, stretch, align);
}

AboutInterface::AboutInterface(QWidget* parent) : QWidget(parent) {
    setObjectName("aboutInterface");
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    auto* content = new QWidget(scroll);
    content->setObjectName("aboutContent");
    scroll->setWidget(content);
    outer->addWidget(scroll);
    auto* lay = new QVBoxLayout(content);
    lay->setContentsMargins(36, 20, 36, 32);
    lay->setSpacing(14);
    lay->addWidget(new SubtitleLabel("关于", content));

    // 主区（常开）：头区 + 运行环境 + 设备兼容
    auto* head = new CardWidget(content);
    auto* hl = new QVBoxLayout(head);
    hl->addWidget(new BodyLabel("<h2 style='margin:0'>AMV Converter</h2>", head));
    const QString ver = QString("v%1").arg(APP_VERSION);
    auto* vb = new PushButton(FluentIcon::COPY, ver, head);
    connect(vb, &QPushButton::clicked, this, [vb, ver] {
        QGuiApplication::clipboard()->setText(ver);
        vb->setText(ver + " 已复制");
    });
    hl->addWidget(vb, 0, Qt::AlignLeft);
    hl->addWidget(new CaptionLabel(
        "面向 Actions ATJ21xx 系列播放器的 AMV/AVI 转换器：自研比特流编码器复刻"
        "原版工具输出格式（逆向 + 实机标定），非简单 FFmpeg 包装。", head));
    auto* rb = new PushButton(FluentIcon::GITHUB, "GitHub 仓库", head);
    connect(rb, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl(REPO_URL));
    });
    hl->addWidget(rb, 0, Qt::AlignLeft);
    lay->addWidget(head);

    auto* env = new CardWidget(content);
    auto* el = new QVBoxLayout(env);
    el->addWidget(new StrongBodyLabel("运行环境", env));
    el->addWidget(new CaptionLabel(
        QString("Qt %1 · %2").arg(qVersion(), QSysInfo::prettyProductName()), env));
    el->addWidget(new CaptionLabel(ffmpegVersionLine(), env));
    el->addWidget(new CaptionLabel("编码内核：C++ 静态编译（含熵编码核）", env));
    lay->addWidget(env);

    auto* dev = new CardWidget(content);
    auto* dl = new QVBoxLayout(dev);
    dl->addWidget(new StrongBodyLabel("设备兼容（实机验证）", dev));
    dl->addWidget(new CaptionLabel(
        "RUIZU M7：128x128@30 / 160x120@30 / 320x240@25 播放与快进全过；"
        "AMV 快进依赖 idx1（已按原版布局写入）。", dev));
    lay->addWidget(dev);

    // 次要内容：折叠分组（默认收起）
    auto* refs = new FoldGroup("本项目使用的第三方库和框架", content);
    for (const Ref& r : REFERENCES) {
        refs->addWidget(new BodyLabel(
            QString("<b>%1</b>（%2）<br><span style='color:gray'>%3</span>")
                .arg(r.name, r.lic, r.desc), refs));
        auto* b = new PushButton(FluentIcon::LINK, "查看仓库", refs);
        const QString url = r.url;
        connect(b, &QPushButton::clicked, this, [url] { QDesktopServices::openUrl(QUrl(url)); });
        refs->addWidget(b, 0, Qt::AlignLeft);
    }
    lay->addWidget(refs);

    auto* thanks = new FoldGroup("致谢", content);
    thanks->addWidget(new CaptionLabel(
        "· 《媒体播放器管理工具 4.46》及其内置 AMV_EncDLL/AMV_DecDLL 的原作者——"
        "格式逆向的行为参照（专有软件，本项目不随附其任何代码与资源）；<br>"
        "· RUIZU M7（ATJ2157）实机——全部比特流结论的验证平台；<br>"
        "· ClassIsland 与 Attic 启动器——关于页内容格式参照。", thanks));
    auto* licBtn = new PushButton(FluentIcon::LINK, "查看本项目 LICENSE（GPL-3.0）", thanks);
    connect(licBtn, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl(QString(REPO_URL) + "/blob/HEAD/LICENSE"));
    });
    thanks->addWidget(licBtn, 0, Qt::AlignLeft);
    lay->addWidget(thanks);

    auto* disc = new FoldGroup("免责声明", content);
    disc->addWidget(new CaptionLabel(
        "本软件按“现状”提供（GPL-3.0），不提供任何明示或暗示的保证；转换与分发"
        "内容的版权责任由使用者承担；对其他 ATJ 型号/固件版本的兼容性未作承诺。<br>"
        "PC 预览建议用本工具预览窗——VLC 3.x 与 PotPlayer 对 AMV 容器存在已知缺陷"
        "（详见仓库 docs/06）。", disc));
    lay->addWidget(disc);
    lay->addStretch(1);
}
