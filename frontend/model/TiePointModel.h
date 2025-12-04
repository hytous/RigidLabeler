#ifndef TIEPOINTMODEL_H
#define TIEPOINTMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QPointF>
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
 * Undo/Redo is now managed externally via QUndoStack in MainWindow.
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

    // Direct point operations (for QUndoCommand use - no internal undo tracking)
    int addFixedPointDirect(const QPointF &point);      // Returns pair index
    int addMovingPointDirect(const QPointF &point);     // Returns pair index
    int addFixedPointDirect(int pairIndex, const QPointF &point);  // Add to specific pair
    int addMovingPointDirect(int pairIndex, const QPointF &point); // Add to specific pair
    void removePointDirect(int pairIndex, bool isFixed);           // Remove specific point
    
    // Query methods
    TiePointPair getPair(int index) const;
    QList<TiePointPair> getAllPairs() const;
    QList<TiePointPair> getCompletePairs() const; // Only pairs with both points
    int pairCount() const;                         // Total pairs (including incomplete)
    int completePairCount() const;                 // Only complete pairs
    ActiveStack getActiveStack() const { return m_activeStack; }
    bool hasBothPoints(int pairIndex) const;       // Check if pair is complete
    int getNextPairIndex() const;                  // Get next available pair index
    
    // Legacy compatibility
    void addTiePoint(const QPointF &fixed, const QPointF &moving);
    void removeTiePoint(int index);
    void insertTiePoint(int index, const QPointF &fixed, const QPointF &moving, int pairIndex = -1);
    int getPairIndexAt(int index) const;  // Get the pairIndex for a given row
    void clearAll();
    int count() const { return pairCount(); }
    TiePoint getTiePoint(int index) const;
    QList<TiePoint> getAllTiePoints() const;
    
    // For display
    void updateFixedPoint(int index, const QPointF &point);
    void updateMovingPoint(int index, const QPointF &point);
    
    // Coordinate display settings
    void setDisplayCoordinateOffset(const QPointF &fixedOffset, const QPointF &movingOffset);
    void setUseTopLeftOrigin(bool useTopLeft);
    bool useTopLeftOrigin() const { return m_useTopLeftOrigin; }

signals:
    void pointAdded(int pairIndex, bool isFixed);
    void pointRemoved(int pairIndex, bool isFixed);
    void pairCompleted(int pairIndex);
    void modelCleared();

private:
    void rebuildPairs();
    int findFixedPointIndex(int pairIndex) const;
    int findMovingPointIndex(int pairIndex) const;
    
    // Separate storage for points
    QList<PointEntry> m_fixedPoints;
    QList<PointEntry> m_movingPoints;
    
    // Current active stack (which was last modified)
    ActiveStack m_activeStack;
    
    // Cached pairs for efficient access
    QList<TiePointPair> m_pairs;
    
    // Coordinate display settings
    bool m_useTopLeftOrigin = false;  // false = center origin (default)
    QPointF m_fixedOffset;            // Image center offset for fixed image
    QPointF m_movingOffset;           // Image center offset for moving image
    
    // Helper for display coordinate conversion
    QPointF toDisplayCoord(const QPointF &pixelPos, bool isFixed) const;
};

#endif // TIEPOINTMODEL_H
