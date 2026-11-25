#pragma once
#include <QThread>
#include <QWidget>

#include <atomic>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>

class QComboBox;
class QDragEnterEvent;
class QDropEvent;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

// 单线程顺序消费队列（转换是 CPU 重活，不做并行）——_QueueWorker 移植
struct Job {
    int row;
    std::string src, dst;
    int w, h, fps;
    std::string mode;  // "amv" | "avi"
    std::shared_ptr<std::atomic<bool>> cancel;
};

class QueueWorker : public QThread {
    Q_OBJECT
public:
    explicit QueueWorker(QObject* parent = nullptr) : QThread(parent) {}
    void enqueue(const Job& j);

signals:
    void progress(int row, int done, int total);
    void finishedRow(int row, bool ok, const QString& msg);

protected:
    void run() override;

private:
    static void progCb(int done, int total, void* user);
    std::mutex mx_;
    std::deque<Job> jobs_;
};

class ConvertInterface : public QWidget {
    Q_OBJECT
public:
    explicit ConvertInterface(QWidget* parent = nullptr);

protected:
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private:
    void build();
    void refreshFps();
    void onMode(int idx);
    void pickOut();
    void addFiles();
    void append(const QStringList& files);
    QString target(const QString& src) const;
    void startAll();
    void onRowClicked(int r, int c);
    void onPreview(int r, int c);
    void onProgress(int r, int done, int total);
    void onDone(int r, bool ok, const QString& msg);
    void clearDone();

    QComboBox* modeCombo_ = nullptr;
    QComboBox* sizeCombo_ = nullptr;
    QComboBox* fpsCombo_ = nullptr;
    QLineEdit* outEdit_ = nullptr;
    QTableWidget* table_ = nullptr;
    QPushButton* startBtn_ = nullptr;
    QPushButton* clearBtn_ = nullptr;
    QLabel* hint_ = nullptr;
    QueueWorker* worker_ = nullptr;
    std::map<int, std::shared_ptr<std::atomic<bool>>> cancels_;
};
