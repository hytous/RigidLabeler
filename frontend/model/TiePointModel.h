#ifndef TIEPOINTMODEL_H
#define TIEPOINTMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QPointF>
#include <QStack>
#include <optional>

/**
 * @brief Represents a single tie point entry (fixed or moving point with index).
 */
struct PointEntry {
    int pairIndex;      // Index of the point pair (0, 1, 2, ...)
    QPointF position;   // Point coordinates
    
    PointEntry() : pairIndex(-1) {}
    PointEntry(int idx, const QPointF& pos) : pairIndex(idx), position(pos) {}
};

/**
 * @brief Represents a complete or partial tie point pair.
 */
struct TiePointPair {
    int index;                          // Pair index
    std::optional<QPointF> fixed;       // Fixed point (may be empty)
    std::optional<QPointF> moving;      // Moving point (may be empty)
    
    TiePointPair() : index(-1) {}
    TiePointPair(int idx) : index(idx) {}
    
    bool hasFixed() const { return fixed.has_value(); }
    bool hasMoving() const { return moving.has_value(); }
    bool isComplete() const { return hasFixed() && hasMoving(); }
};

// Legacy compatibility
struct TiePoint {
    QPointF fixed;
    QPointF moving;
    TiePoint() = default;
    TiePoint(const QPointF& f, const QPointF& m) : fixed(f), moving(m) {}
};

/**
 * @brief Enum to identify which image/stack is active.
 */
enum class ActiveStack {
    None,
    Fixed,
    Moving
};

/**
 * @brief Model for managing tie points between two images.
 * 
 * Uses separate storage for fixed and moving points, with index-based pairing.
 * Supports partial pairs (only fixed or only moving point set).
 * Provides undo capability for individual point additions.
 */
class TiePointModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column {
        ColIndex = 0,
        ColFixedX,
        ColFixedY,
        ColMovingX,
        ColMovingY,
        ColCount
    };

    explicit TiePointModel(QObject *parent = nullptr);

    // QAbstractTableModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    // New point management API
    int addFixedPoint(const QPointF &point);      // Returns pair index
    int addMovingPoint(const QPointF &point);     // Returns pair index
    bool undoLastPoint();                          // Undo last added point (fixed or moving)
    bool redoLastPoint();                          // Redo last undone point
    
    // Query methods
    TiePointPair getPair(int index) const;
    QList<TiePointPair> getAllPairs() const;
    QList<TiePointPair> getCompletePairs() const; // Only pairs with both points
    int pairCount() const;                         // Total pairs (including incomplete)
    int completePairCount() const;                 // Only complete pairs
    ActiveStack getActiveStack() const { return m_activeStack; }
    bool canUndo() const;
    bool canRedo() const;
    bool hasBothPoints(int pairIndex) const;       // Check if pair is complete
    
    // Legacy compatibility
    void addTiePoint(const QPointF &fixed, const QPointF &moving);
    void removeTiePoint(int index);
    void insertTiePoint(int index, const QPointF &fixed, const QPointF &moving);
    void clearAll();
    int count() const { return pairCount(); }
    TiePoint getTiePoint(int index) const;
    QList<TiePoint> getAllTiePoints() const;
    
    // For display
    void updateFixedPoint(int index, const QPointF &point);
    void updateMovingPoint(int index, const QPointF &point);

signals:
    void pointAdded(int pairIndex, bool isFixed);
    void pointRemoved(int pairIndex, bool isFixed);
    void pairCompleted(int pairIndex);
    void modelCleared();
    void undoRedoStateChanged();

private:
    void rebuildPairs();
    int getNextPairIndex() const;
    int findFixedPointIndex(int pairIndex) const;
    int findMovingPointIndex(int pairIndex) const;
    
    // Separate storage for points
    QList<PointEntry> m_fixedPoints;
    QList<PointEntry> m_movingPoints;
    
    // Undo/Redo stacks - store operation info
    struct UndoEntry {
        int pairIndex;
        bool isFixed;
        QPointF position;
    };
    QStack<UndoEntry> m_undoStack;
    QStack<UndoEntry> m_redoStack;
    
    // Current active stack (which was last modified)
    ActiveStack m_activeStack;
    
    // Cached pairs for efficient access
    QList<TiePointPair> m_pairs;
};

#endif // TIEPOINTMODEL_H
