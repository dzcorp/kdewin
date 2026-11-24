#include "lumenstyle.h"

#include <QStylePlugin>

namespace Lumen
{
class StylePlugin : public QStylePlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QStyleFactoryInterface_iid FILE "lumen.json")
public:
    QStyle *create(const QString &key) override
    {
        return key.compare(QLatin1String("lumen"), Qt::CaseInsensitive) == 0 ? new Style : nullptr;
    }
};
}

#include "lumenplugin.moc"
