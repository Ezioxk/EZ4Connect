#include <QCoreApplication>
#include <QDebug>

#include "infrastructure/coreprocess/coreoutputbuffer.h"
#include "infrastructure/coreprocess/coreoutputparser.h"

namespace
{
bool recognizesEveryTotpSplit()
{
    for (const QByteArray prompt : {QByteArray("Please enter the TOTP token:"),
                                   QByteArray("Please enter your TOTP code:")})
    {
        for (qsizetype split = 0; split <= prompt.size(); ++split)
        {
            CoreOutputBuffer buffer;
            int requests = 0;
            const auto append = [&](const QByteArray &chunk)
            {
                auto lines = buffer.append(chunk);
                if (CoreOutputParser::hasInteractivePrompt(buffer.pendingData()))
                    lines.append(buffer.takePendingData());
                for (const auto &line : lines)
                    if (CoreOutputParser::parse(QString::fromUtf8(line)) == CoreOutputEvent::TotpCode)
                        ++requests;
            };
            append(prompt.first(split));
            append(prompt.sliced(split));
            append("\r\n");
            if (requests != 1) return false;
        }
    }
    return true;
}
bool buffersFragmentedLines()
{
    CoreOutputBuffer buffer;
    const bool firstChunkBuffered = buffer.append("first half").isEmpty();
    const QList<QByteArray> lines = buffer.append(" second half\nnext");
    const bool completeLineReturned = lines == QList<QByteArray>{"first half second half"};
    const bool remainderPreserved = buffer.pendingData() == "next";

    if (!firstChunkBuffered || !completeLineReturned || !remainderPreserved)
    {
        qCritical() << "buffersFragmentedLines failed";
        return false;
    }
    return true;
}

bool extractsMultipleLinesAndNormalizesCrLf()
{
    CoreOutputBuffer buffer;
    const QList<QByteArray> lines = buffer.append("first\r\nsecond\n\n");
    const QList<QByteArray> expected{"first", "second", ""};
    if (lines != expected || buffer.hasPendingData())
    {
        qCritical() << "extractsMultipleLinesAndNormalizesCrLf failed";
        return false;
    }
    return true;
}

bool preservesSplitUtf8Characters()
{
    CoreOutputBuffer buffer;
    const QByteArray output = QStringLiteral("日志内容").toUtf8() + '\n';
    const qsizetype splitPosition = 2;

    const bool firstChunkBuffered = buffer.append(output.first(splitPosition)).isEmpty();
    const QList<QByteArray> lines = buffer.append(output.sliced(splitPosition));
    if (!firstChunkBuffered || lines != QList<QByteArray>{QStringLiteral("日志内容").toUtf8()})
    {
        qCritical() << "preservesSplitUtf8Characters failed";
        return false;
    }
    return true;
}

bool takesPendingData()
{
    CoreOutputBuffer buffer;
    buffer.append("prompt without newline");
    const QByteArray pending = buffer.takePendingData();
    if (pending != "prompt without newline" || buffer.hasPendingData())
    {
        qCritical() << "takesPendingData failed";
        return false;
    }
    return true;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    return buffersFragmentedLines()
               && recognizesEveryTotpSplit()
               && extractsMultipleLinesAndNormalizesCrLf()
               && preservesSplitUtf8Characters()
               && takesPendingData()
           ? 0
           : 1;
}
