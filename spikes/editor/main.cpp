// SPDX-License-Identifier: GPL-3.0-or-later
// M1.2 editor feasibility spike — measures the risks listed in docs/specs/sql-editor.md §10 for a
// QML TextArea + KSyntaxHighlighting editor on Qt 6.12. Throwaway code.
//
//   editor_spike --bench [--size-mb 5]   run all measurements, print a Markdown report
//   editor_spike [--size-mb 5]           interactive: try typing in the highlighted script
#include "sqlcorpus.h"

#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QInputMethodEvent>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickTextDocument>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTest>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>

#include <KSyntaxHighlighting/AbstractHighlighter>
#include <KSyntaxHighlighting/Definition>
#include <KSyntaxHighlighting/Format>
#include <KSyntaxHighlighting/Repository>
#include <KSyntaxHighlighting/State>
#include <KSyntaxHighlighting/SyntaxHighlighter>
#include <KSyntaxHighlighting/Theme>
#include <algorithm>
#include <cstdio>
#include <vector>

using namespace Qt::StringLiterals;
namespace KSH = KSyntaxHighlighting;

namespace {

QString g_report;
void line(const QString &text)
{
    g_report += text + u'\n';
    std::printf("%s\n", qPrintable(text));
    std::fflush(stdout);
}

double ms(const QElapsedTimer &t)
{
    return static_cast<double>(t.nsecsElapsed()) / 1e6;
}

qint64 rssMb()
{
    QFile f(u"/proc/self/status"_s);
    if (!f.open(QIODevice::ReadOnly))
        return -1;
    for (const QByteArray &l : f.readAll().split('\n')) {
        if (l.startsWith("VmRSS:"))
            return l.mid(6).trimmed().split(' ').value(0).toLongLong() / 1024;
    }
    return -1;
}

// Time until the next frame has been presented (or -1 on timeout).
double waitFrame(QQuickWindow *window, int timeoutMs = 20000)
{
    QElapsedTimer t;
    t.start();
    QSignalSpy spy(window, &QQuickWindow::frameSwapped);
    window->update();
    if (!spy.wait(timeoutMs))
        return -1;
    return ms(t);
}

struct Stats
{
    double p50 = 0, p95 = 0, max = 0;
};

Stats stats(std::vector<double> v)
{
    if (v.empty())
        return {};
    std::sort(v.begin(), v.end());
    const auto at = [&v](double q) {
        return v[std::min(v.size() - 1, static_cast<size_t>(q * static_cast<double>(v.size())))];
    };
    return {at(0.5), at(0.95), v.back()};
}

QRectF rectAt(QQuickItem *area, int position)
{
    QRectF rect;
    QMetaObject::invokeMethod(area, "positionToRectangle", Q_RETURN_ARG(QRectF, rect),
                              Q_ARG(int, position));
    return rect;
}

// Key press -> next presented frame, in ms.
double typeKey(QQuickWindow *window, Qt::Key key, const QString &text = {})
{
    QElapsedTimer t;
    t.start();
    QSignalSpy spy(window, &QQuickWindow::frameSwapped);
    if (text.isEmpty())
        QTest::keyClick(window, key);
    else
        QTest::sendKeyEvent(QTest::Click, window, key, text, Qt::NoModifier);
    if (spy.isEmpty() && !spy.wait(20000))
        return -1;
    return ms(t);
}

struct Editor
{
    QQuickWindow *window = nullptr;
    QQuickItem *area = nullptr;
    QTextDocument *doc = nullptr;
    KSH::SyntaxHighlighter *highlighter = nullptr;
    KSH::Repository repository;
};

bool setUp(QQmlApplicationEngine &engine, Editor &e)
{
    engine.loadFromModule("EditorSpike", "Spike");
    e.window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    if (!e.window || !QTest::qWaitForWindowExposed(e.window))
        return false;
    e.area = e.window->findChild<QQuickItem *>(u"area"_s);
    auto *qtd = e.area->property("textDocument").value<QQuickTextDocument *>();
    e.doc = qtd ? qtd->textDocument() : nullptr;
    return e.doc != nullptr;
}

// Variant G: tokenize with KSyntaxHighlighting's AbstractHighlighter, set each block's formats
// directly with QTextLayout::setFormats, and mark the document dirty once (instead of
// QSyntaxHighlighter's per-block markContentsDirty, which relayouts everything below each time).
class BatchHighlighter : public KSH::AbstractHighlighter
{
public:
    void highlightDocument(QTextDocument *doc)
    {
        KSH::State state;
        for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
            m_ranges.clear();
            state = highlightLine(block.text(), state);
            block.layout()->setFormats(m_ranges);
        }
        doc->markContentsDirty(0, doc->characterCount());
    }

protected:
    void applyFormat(int offset, int length, const KSH::Format &format) override
    {
        if (length == 0 || format.isDefaultTextStyle(theme()))
            return;
        QTextCharFormat tf;
        if (format.hasTextColor(theme()))
            tf.setForeground(format.textColor(theme()));
        if (format.hasBackgroundColor(theme()))
            tf.setBackground(format.backgroundColor(theme()));
        if (format.isBold(theme()))
            tf.setFontWeight(QFont::Bold);
        if (format.isItalic(theme()))
            tf.setFontItalic(true);
        m_ranges.append({offset, length, tf});
    }

private:
    QList<QTextLayout::FormatRange> m_ranges;
};

void bench(qsizetype sizeMb)
{
    QQmlApplicationEngine engine;
    Editor e;
    if (!setUp(engine, e)) {
        line(u"setup failed"_s);
        return;
    }
    const QString corpus = makeSqlCorpus(sizeMb * 1024 * 1024);
    line(u"# M1.2 editor spike — %1, Qt %2, %3"_s.arg(
        QGuiApplication::platformName(), QString::fromLatin1(qVersion()),
        e.window->rendererInterface()->graphicsApi() == QSGRendererInterface::Software
            ? u"software renderer"_s
            : u"GPU renderer"_s));
    line(u"Corpus: %1 MB, %2 chars"_s.arg(sizeMb).arg(corpus.size()));
    line(u"\n| Measurement | Result |\n|---|---|"_s);
    const qint64 rss0 = rssMb();

    // 1. Load
    // 1+2. Load with the highlighter already attached (variant B: one highlighting pass).
    e.highlighter = new KSH::SyntaxHighlighter(e.doc);
    e.highlighter->setDefinition(e.repository.definitionForName(u"SQL (PostgreSQL)"_s));
    e.highlighter->setTheme(e.repository.defaultTheme(KSH::Repository::DarkTheme));
    QElapsedTimer t;
    t.start();
    e.area->setProperty("text", corpus);
    QCoreApplication::processEvents();
    const double setTextMs = ms(t);
    const double firstFrame = waitFrame(e.window);
    line(u"| Load + highlight (highlighter attached first) | %1 ms |"_s.arg(setTextMs, 0, 'f', 0));
    line(u"| Load: next frame | %1 ms |"_s.arg(firstFrame, 0, 'f', 0));
    line(u"| Blocks (lines) | %1 |"_s.arg(e.doc->blockCount()));
    line(u"| Memory after load + highlight (RSS delta) | %1 MB |"_s.arg(rssMb() - rss0));

    const QTextBlock probe = e.doc->findBlockByNumber(5);
    line(u"| Highlight formats present on a block | %1 |"_s.arg(
        probe.layout()->formats().isEmpty()
            ? u"**no**"_s
            : u"yes (%1 ranges)"_s.arg(probe.layout()->formats().size())));

    // 3. Geometry lookups for overlays
    const int mid = static_cast<int>(corpus.size() / 2);
    t.restart();
    double sink = 0;
    for (int i = 0; i < 1000; ++i)
        sink += rectAt(e.area, mid + i * 7).y();
    line(u"| positionToRectangle (avg of 1000, mid-document) | %1 µs |"_s.arg(ms(t), 0, 'f', 1));
    Q_UNUSED(sink)

    // 4. Typing latency
    e.area->forceActiveFocus();
    const QTextBlock midBlock = e.doc->findBlock(mid);
    e.area->setProperty("cursorPosition", midBlock.position() + 4);
    waitFrame(e.window);
    for (int i = 0; i < 3; ++i)
        typeKey(e.window, Qt::Key_A); // warm-up
    std::vector<double> samples;
    for (int i = 0; i < 50; ++i)
        samples.push_back(typeKey(e.window, Qt::Key_A));
    const Stats mid50 = stats(samples);
    line(u"| Typing mid-document, key→frame p50 / p95 / max | %1 / %2 / %3 ms |"_s
             .arg(mid50.p50, 0, 'f', 1)
             .arg(mid50.p95, 0, 'f', 1)
             .arg(mid50.max, 0, 'f', 1));

    e.area->setProperty("cursorPosition", 10);
    samples.clear();
    for (int i = 0; i < 30; ++i)
        samples.push_back(typeKey(e.window, Qt::Key_B));
    const Stats start = stats(samples);
    line(u"| Typing at document start, p50 / p95 / max | %1 / %2 / %3 ms |"_s
             .arg(start.p50, 0, 'f', 1)
             .arg(start.p95, 0, 'f', 1)
             .arg(start.max, 0, 'f', 1));

    // Worst case: opening a block comment re-highlights everything after it.
    e.area->setProperty("cursorPosition", midBlock.position());
    const double slash = typeKey(e.window, Qt::Key_Slash, u"/"_s);
    const double star = typeKey(e.window, Qt::Key_Asterisk, u"*"_s);
    line(u"| Typing `/*` mid-document (re-highlights the rest) | `/` %1 ms, `*` %2 ms |"_s
             .arg(slash, 0, 'f', 0)
             .arg(star, 0, 'f', 0));
    t.restart();
    QTest::keyClick(e.window, Qt::Key_Z, Qt::ControlModifier);
    QTest::keyClick(e.window, Qt::Key_Z, Qt::ControlModifier);
    waitFrame(e.window);
    line(u"| Undo `/*` (Ctrl+Z ×2) → frame | %1 ms |"_s.arg(ms(t), 0, 'f', 0));

    // Scroll to the end
    t.restart();
    e.area->setProperty("cursorPosition", e.doc->characterCount() - 1);
    waitFrame(e.window);
    line(u"| Jump to end of document → frame | %1 ms |"_s.arg(ms(t), 0, 'f', 0));
    line(u"| Memory at end of typing tests (RSS delta) | %1 MB |"_s.arg(rssMb() - rss0));

    // Theme switch = full re-highlight of a laid-out document, with the batch highlighter (G).
    e.highlighter->setDocument(nullptr);
    {
        BatchHighlighter batch;
        batch.setDefinition(e.repository.definitionForName(u"SQL (PostgreSQL)"_s));
        batch.setTheme(e.repository.defaultTheme(KSH::Repository::LightTheme));
        t.restart();
        batch.highlightDocument(e.doc);
        waitFrame(e.window);
        line(u"| Theme switch: full re-highlight with batch highlighter → frame | %1 ms |"_s.arg(
            ms(t), 0, 'f', 0));
    }
    // (QSyntaxHighlighter stays detached: re-attaching queues the slow per-block rehighlight.)

    // 5. Folding: does QQuickTextEdit honour hidden blocks?
    {
        const int first = e.doc->findBlock(mid).blockNumber() + 1;
        const QTextBlock below = e.doc->findBlockByNumber(first + 300);
        const double y0 = rectAt(e.area, below.position()).y();
        QTextBlock b = e.doc->findBlockByNumber(first);
        const int from = b.position();
        for (int i = 0; i < 100 && b.isValid(); ++i, b = b.next())
            b.setVisible(false);
        e.doc->markContentsDirty(from, b.position() - from);
        waitFrame(e.window);
        const double y1 = rectAt(e.area, below.position()).y();
        const double lineHeight = rectAt(e.area, below.position()).height();
        const bool collapsed = (y0 - y1) > 50 * lineHeight;
        line(u"| Folding: hide 100 lines, text below moves up | %1 (y %2 → %3) |"_s
                 .arg(collapsed ? u"**yes**"_s : u"**no**"_s)
                 .arg(y0, 0, 'f', 0)
                 .arg(y1, 0, 'f', 0));
        b = e.doc->findBlockByNumber(first);
        for (int i = 0; i < 100 && b.isValid(); ++i, b = b.next())
            b.setVisible(true);
        e.doc->markContentsDirty(from, b.position() - from);
    }

    // 6. Undo grouping for auto-close, and input-method composition
    {
        e.area->setProperty("text", u"SELECT "_s);
        e.area->setProperty("cursorPosition", 7);
        e.area->forceActiveFocus();
        QTextCursor c(e.doc);
        c.setPosition(7);
        c.beginEditBlock();
        c.insertText(u"()"_s);
        c.endEditBlock();
        e.area->setProperty("cursorPosition", 8);
        const bool canUndo = e.area->property("canUndo").toBool();
        QMetaObject::invokeMethod(e.area, "undo");
        const QString afterUndo = e.area->property("text").toString();
        line(
            u"| Auto-close `()` as one QTextCursor edit block, TextArea.undo() once | canUndo=%1, text after undo `%2` → %3 |"_s
                .arg(canUndo ? u"true"_s : u"false"_s, afterUndo,
                     afterUndo == u"SELECT "_s ? u"**one step**"_s : u"**not grouped**"_s));

        e.area->setProperty("text", u"SELECT 1; -- "_s);
        e.area->setProperty("cursorPosition", 13);
        QObject *focus = QGuiApplication::focusObject();
        QInputMethodEvent preedit(u"ni"_s, {});
        QCoreApplication::sendEvent(focus, &preedit);
        const QString duringPreedit = e.area->property("preeditText").toString();
        QInputMethodEvent commit;
        commit.setCommitString(u"你好"_s);
        QCoreApplication::sendEvent(focus, &commit);
        const QString committed = e.area->property("text").toString();
        QMetaObject::invokeMethod(e.area, "undo");
        const QString undone = e.area->property("text").toString();
        line(
            u"| IME: preedit `ni` shown, commit `你好`, undo | preedit=`%1`, after commit `%2`, after undo `%3` |"_s
                .arg(duringPreedit, committed, undone));
    }

    line(u"| Peak memory (RSS delta at end) | %1 MB |"_s.arg(rssMb() - rss0));
}

// Where does highlighting time go? Same corpus, three set-ups.
void highlightVariants(double sizeMb)
{
    const QString corpus = makeSqlCorpus(static_cast<qsizetype>(sizeMb * 1024 * 1024));
    line(u"\n## Highlight variants — %1 MB, %2 lines"_s.arg(sizeMb).arg(corpus.count(u'\n')));
    KSH::Repository repository;
    const auto def = repository.definitionForName(u"SQL (PostgreSQL)"_s);
    const auto theme = repository.defaultTheme(KSH::Repository::DarkTheme);
    QElapsedTimer t;

    { // A: no view at all
        QTextDocument doc;
        doc.setPlainText(corpus);
        KSH::SyntaxHighlighter h;
        h.setDefinition(def);
        h.setTheme(theme);
        t.start();
        h.setDocument(&doc);
        h.rehighlight();
        line(u"| A. QTextDocument without a view | %1 ms |"_s.arg(ms(t), 0, 'f', 0));
    }
    { // B: highlighter attached before the text is set
        QQmlApplicationEngine engine;
        Editor e;
        setUp(engine, e);
        e.highlighter = new KSH::SyntaxHighlighter(e.doc);
        e.highlighter->setDefinition(def);
        e.highlighter->setTheme(theme);
        t.start();
        e.area->setProperty("text", corpus);
        QCoreApplication::processEvents();
        waitFrame(e.window);
        line(u"| B. TextArea, highlighter attached before setText (load + highlight) | %1 ms |"_s
                 .arg(ms(t), 0, 'f', 0));
    }
    { // C: rehighlight while the TextArea is hidden
        QQmlApplicationEngine engine;
        Editor e;
        setUp(engine, e);
        e.area->setProperty("text", corpus);
        waitFrame(e.window);
        e.highlighter = new KSH::SyntaxHighlighter(e.doc);
        e.highlighter->setDefinition(def);
        e.highlighter->setTheme(theme);
        e.area->setVisible(false);
        t.start();
        e.highlighter->rehighlight();
        e.area->setVisible(true);
        waitFrame(e.window);
        line(u"| C. TextArea hidden during rehighlight | %1 ms |"_s.arg(ms(t), 0, 'f', 0));
    }
    { // D: document signals blocked during rehighlight, one change notification afterwards
        QQmlApplicationEngine engine;
        Editor e;
        setUp(engine, e);
        e.area->setProperty("text", corpus);
        waitFrame(e.window);
        e.highlighter = new KSH::SyntaxHighlighter(e.doc);
        e.highlighter->setDefinition(def);
        e.highlighter->setTheme(theme);
        t.start();
        e.doc->blockSignals(true);
        e.highlighter->rehighlight();
        e.doc->blockSignals(false);
        const double hl = ms(t);
        e.doc->markContentsDirty(0, e.doc->characterCount());
        waitFrame(e.window);
        const QTextBlock probe = e.doc->findBlockByNumber(5);
        line(
            u"| D. Signals blocked during rehighlight (+1 markContentsDirty) | %1 ms (rehighlight %2 ms), formats on block: %3 |"_s
                .arg(ms(t), 0, 'f', 0)
                .arg(hl, 0, 'f', 0)
                .arg(probe.layout()->formats().isEmpty() ? u"no"_s : u"yes"_s));
    }
    { // G: own batch highlighter on an already laid-out TextArea
        QQmlApplicationEngine engine;
        Editor e;
        setUp(engine, e);
        e.area->setProperty("text", corpus);
        waitFrame(e.window);
        BatchHighlighter h;
        h.setDefinition(def);
        h.setTheme(theme);
        t.start();
        h.highlightDocument(e.doc);
        const double hl = ms(t);
        waitFrame(e.window);
        const QTextBlock probe = e.doc->findBlockByNumber(5);
        line(
            u"| G. Own batch highlighter (setFormats + one dirty mark) → frame | %1 ms (highlight %2 ms), formats on block: %3 |"_s
                .arg(ms(t), 0, 'f', 0)
                .arg(hl, 0, 'f', 0)
                .arg(probe.layout()->formats().isEmpty()
                         ? u"no"_s
                         : u"yes (%1)"_s.arg(probe.layout()->formats().size())));
    }
}

// Typing latency without any highlighter, per document size: is the cost QQuickTextEdit itself?
void typingOnly(const QList<double> &sizes)
{
    line(u"\n## Typing without a highlighter (key→frame, mid-document)"_s);
    line(u"| Size | p50 | p95 | max |\n|---|---|---|---|"_s);
    for (const double sizeMb : sizes) {
        QQmlApplicationEngine engine;
        Editor e;
        setUp(engine, e);
        const QString corpus = makeSqlCorpus(static_cast<qsizetype>(sizeMb * 1024 * 1024));
        e.area->setProperty("text", corpus);
        waitFrame(e.window);
        e.area->forceActiveFocus();
        e.area->setProperty("cursorPosition",
                            e.doc->findBlock(static_cast<int>(corpus.size() / 2)).position() + 4);
        waitFrame(e.window);
        for (int i = 0; i < 3; ++i)
            typeKey(e.window, Qt::Key_A);
        std::vector<double> samples;
        for (int i = 0; i < 40; ++i)
            samples.push_back(typeKey(e.window, Qt::Key_A));
        const Stats st = stats(samples);
        line(u"| %1 MB | %2 ms | %3 ms | %4 ms |"_s.arg(sizeMb)
                 .arg(st.p50, 0, 'f', 1)
                 .arg(st.p95, 0, 'f', 1)
                 .arg(st.max, 0, 'f', 1));
    }
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption benchOption(u"bench"_s, u"Run measurements and exit."_s);
    const QCommandLineOption variantsOption(u"variants"_s,
                                            u"Highlight variants at 0.25 and 0.5 MB."_s);
    const QCommandLineOption typingOption(u"typing"_s,
                                          u"Typing latency without highlighter at 0.5-5 MB."_s);
    const QCommandLineOption sizeOption(u"size-mb"_s, u"Script size in MB (default 5)."_s, u"mb"_s,
                                        u"5"_s);
    parser.addOptions({benchOption, variantsOption, typingOption, sizeOption});
    parser.process(app);
    const qsizetype sizeMb = parser.value(sizeOption).toInt();

    if (parser.isSet(typingOption)) {
        typingOnly({0.5, 1, 2, 5});
        return 0;
    }
    if (parser.isSet(variantsOption)) {
        highlightVariants(0.5);
        highlightVariants(1);
        return 0;
    }
    if (parser.isSet(benchOption)) {
        bench(sizeMb);
        return 0;
    }

    QQmlApplicationEngine engine;
    Editor e;
    if (!setUp(engine, e))
        return 1;
    e.area->setProperty("text", makeSqlCorpus(sizeMb * 1024 * 1024));
    e.highlighter = new KSH::SyntaxHighlighter(e.doc);
    e.highlighter->setDefinition(e.repository.definitionForName(u"SQL (PostgreSQL)"_s));
    e.highlighter->setTheme(e.repository.defaultTheme(KSH::Repository::DarkTheme));
    e.area->forceActiveFocus();
    return QGuiApplication::exec();
}
