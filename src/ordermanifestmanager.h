#ifndef ORDERMANIFESTMANAGER_H
#define ORDERMANIFESTMANAGER_H

enum class ManifestParseResult {
    FILE_NOT_EXIST,
    FILE_CORRUPTED,
    POSITIVE_WITH_WARNINGS,
    POSITIVE
};

class ManifestKeys {
public:
    enum Value : uint8_t
    {
        SIZE,
        SORT_TYPE,
        TILING,
        MIN_EXPOSURE,
        STEP,
        SPACE,
        ENTITIES,
    };

    static const QString strings[7];

    ManifestKeys() = default;
    constexpr ManifestKeys(Value key) : value(key) { }

    constexpr const QString& toQString() {
        return strings[value];
    }
  private:
    Value value;
};


class ManifestSortType {
public:
    enum Value : uint8_t
    {
        NUMERIC,
        ALPHANUMERIC,
        CUSTOM
    };

    static const QString strings[3];

    ManifestSortType() = default;
    constexpr ManifestSortType(Value key) : value(key) { }

    ManifestSortType(QString key) {
        for(uint8_t i=0; i<strings->length(); ++i) {
            if(strings[i] == key) {
                value = (Value)i;

                return;
            }
        }

        value = CUSTOM;
    }

    constexpr const QString& toQString() {
        return strings[value];
    }
private:
    Value value;
};


class OrderManifestManager {

public:
    class Iterator {
    public:
        Iterator(QStringList list) : _list(list) { _ptr = 0; }

        void     operator++() { _ptr++; }
        QString  operator*()  {
            return _list[_ptr];
        }
        bool     hasNext()    {
            return _ptr + 1 < _list.size();
        }
    private:
            int         _ptr;
            QStringList _list;
    };

    OrderManifestManager()      { }
    ~OrderManifestManager()     { }

    void setPath ( QString path ) {
        _dirPath = path;
    }

    QString path() { return _dirPath; }

    ManifestSortType sortType() { return _type; }

    void setSortType( ManifestSortType sortType) { this->_type = sortType;  }

    void setFileList ( QStringList list ) {
        this->_fileNameList.clear();
        this->_fileNameList.append(list);

        this->_size = list.size();
    }

    void setTilingMinExpoTime(int minExoTm) {
        this->_tilingMinExposure = minExoTm;
    }

    void setTilingStep (int step) {
        this->_tilingStep = step;
    }

    void setTilingSpace (double space) {
        this->_tilingSpace = space;
    }

    void setTiled (bool tiled) {
        this->_tiled = tiled;
    }

    inline bool tiled()             { return _tiled; }
    inline int tilingMinExposure()  { return _tilingMinExposure; }
    inline int tilingStep()         { return _tilingStep; }
    inline double tilingSpace()     { return _tilingSpace; }

    QString getFirstElement() { return _fileNameList.size() > 0 ? _fileNameList[0] : nullptr; }

    ManifestParseResult parse(QStringList *errors, QStringList *warningList);

    bool save();

    Iterator iterator() {
        return Iterator(_fileNameList);
    }


private:
    QString             _dirPath;
    ManifestSortType    _type;
    int                 _size;
    bool                _tiled             { false };
    int                 _tilingMinExposure { 2 };
    int                 _tilingStep        { 2 };
    double              _tilingSpace       { 0.25L };
    QStringList         _fileNameList      { };
};

#endif // ORDERMANIFESTMANAGER_H
