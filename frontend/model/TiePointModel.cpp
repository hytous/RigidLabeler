#include "TiePointModel.h"
#include <QColor>

TiePointModel::TiePointModel(QObject *parent)
    : QAbstractTableModel(parent)
    , m_activeStack(ActiveStack::None)
{
}

// ============================================================================
// QAbstractTableModel Interface
// ============================================================================

int TiePointModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_pairs.count();
}

int TiePointModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return ColCount;
}

QVariant TiePointModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_pairs.count())
        return QVariant();

    const TiePointPair &pair = m_pairs.at(index.row());

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
        case ColIndex:
            return index.row() + 1;  // 1-based index
        case ColFixedX:
            return pair.hasFixed() ? QString::number(pair.fixed->x(), 'f', 2) : QString("-");
        case ColFixedY:
            return pair.hasFixed() ? QString::number(pair.fixed->y(), 'f', 2) : QString("-");
        case ColMovingX:
            return pair.hasMoving() ? QString::number(pair.moving->x(), 'f', 2) : QString("-");
        case ColMovingY:
            return pair.hasMoving() ? QString::number(pair.moving->y(), 'f', 2) : QString("-");
        }
    }

    if (role == Qt::TextAlignmentRole) {
        return Qt::AlignCenter;
    }
    
    // Gray out incomplete entries
    if (role == Qt::ForegroundRole) {
        if ((index.column() == ColFixedX || index.column() == ColFixedY) && !pair.hasFixed()) {
            return QColor(Qt::gray);
        }
        if ((index.column() == ColMovingX || index.column() == ColMovingY) && !pair.hasMoving()) {
            return QColor(Qt::gray);
        }
    }

    return QVariant();
}

QVariant TiePointModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Horizontal) {
        switch (section) {
        case ColIndex:
            return tr("#");
        case ColFixedX:
            return tr("Fixed X");
        case ColFixedY:
            return tr("Fixed Y");
        case ColMovingX:
            return tr("Moving X");
        case ColMovingY:
            return tr("Moving Y");
        }
    }

    return QVariant();
}

Qt::ItemFlags TiePointModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;

    Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    
    // Allow editing of coordinate columns (not index) only if the point exists
    if (index.column() != ColIndex && index.row() < m_pairs.count()) {
        const TiePointPair &pair = m_pairs.at(index.row());
        if ((index.column() == ColFixedX || index.column() == ColFixedY) && pair.hasFixed()) {
            flags |= Qt::ItemIsEditable;
        }
        if ((index.column() == ColMovingX || index.column() == ColMovingY) && pair.hasMoving()) {
            flags |= Qt::ItemIsEditable;
        }
    }

    return flags;
}

bool TiePointModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || role != Qt::EditRole)
        return false;

    if (index.row() >= m_pairs.count())
        return false;

    bool ok;
    double val = value.toDouble(&ok);
    if (!ok)
        return false;

    int pairIndex = m_pairs.at(index.row()).index;

    switch (index.column()) {
    case ColFixedX:
    case ColFixedY: {
        int idx = findFixedPointIndex(pairIndex);
        if (idx >= 0) {
            if (index.column() == ColFixedX)
                m_fixedPoints[idx].position.setX(val);
            else
                m_fixedPoints[idx].position.setY(val);
        }
        break;
    }
    case ColMovingX:
    case ColMovingY: {
        int idx = findMovingPointIndex(pairIndex);
        if (idx >= 0) {
            if (index.column() == ColMovingX)
                m_movingPoints[idx].position.setX(val);
            else
                m_movingPoints[idx].position.setY(val);
        }
        break;
    }
    default:
        return false;
    }

    rebuildPairs();
    emit dataChanged(index, index, {role});
    return true;
}

// ============================================================================
// New Point Management API
// ============================================================================

int TiePointModel::addFixedPoint(const QPointF &point)
{
    // Find a pair that needs a fixed point (has moving but no fixed)
    int pairIndex = -1;
    
    for (const TiePointPair &pair : m_pairs) {
        if (pair.hasMoving() && !pair.hasFixed()) {
            pairIndex = pair.index;
            break;
        }
    }
    
    // If no incomplete pair found, create new pair index
    if (pairIndex < 0) {
        pairIndex = getNextPairIndex();
    }
    
    // Add to fixed points
    m_fixedPoints.append(PointEntry(pairIndex, point));
    
    // Clear redo stack on new action
    m_redoStack.clear();
    
    // Push to undo stack
    UndoEntry entry;
    entry.pairIndex = pairIndex;
    entry.isFixed = true;
    entry.position = point;
    m_undoStack.push(entry);
    
    // Set active stack
    m_activeStack = ActiveStack::Fixed;
    
    // Rebuild pairs and update view
    rebuildPairs();
    
    // Check if pair is now complete
    for (const TiePointPair &pair : m_pairs) {
        if (pair.index == pairIndex && pair.isComplete()) {
            emit pairCompleted(pairIndex);
            break;
        }
    }
    
    emit pointAdded(pairIndex, true);
    emit undoRedoStateChanged();
    
    return pairIndex;
}

int TiePointModel::addMovingPoint(const QPointF &point)
{
    // Find the pair index that needs a moving point
    int pairIndex = -1;
    
    // Look for a pair that has fixed but no moving
    for (const TiePointPair &pair : m_pairs) {
        if (pair.hasFixed() && !pair.hasMoving()) {
            pairIndex = pair.index;
            break;
        }
    }
    
    // If no incomplete pair found, create new pair index
    if (pairIndex < 0) {
        pairIndex = getNextPairIndex();
    }
    
    // Add to moving points
    m_movingPoints.append(PointEntry(pairIndex, point));
    
    // Clear redo stack on new action
    m_redoStack.clear();
    
    // Push to undo stack
    UndoEntry entry;
    entry.pairIndex = pairIndex;
    entry.isFixed = false;
    entry.position = point;
    m_undoStack.push(entry);
    
    // Set active stack
    m_activeStack = ActiveStack::Moving;
    
    // Rebuild pairs and update view
    rebuildPairs();
    
    // Check if pair is now complete
    for (const TiePointPair &pair : m_pairs) {
        if (pair.index == pairIndex && pair.isComplete()) {
            emit pairCompleted(pairIndex);
            break;
        }
    }
    
    emit pointAdded(pairIndex, false);
    emit undoRedoStateChanged();
    
    return pairIndex;
}

bool TiePointModel::undoLastPoint()
{
    if (m_undoStack.isEmpty())
        return false;
    
    UndoEntry entry = m_undoStack.pop();
    
    // Remove the point from the appropriate list
    if (entry.isFixed) {
        for (int i = m_fixedPoints.size() - 1; i >= 0; --i) {
            if (m_fixedPoints[i].pairIndex == entry.pairIndex) {
                m_fixedPoints.removeAt(i);
                break;
            }
        }
    } else {
        for (int i = m_movingPoints.size() - 1; i >= 0; --i) {
            if (m_movingPoints[i].pairIndex == entry.pairIndex) {
                m_movingPoints.removeAt(i);
                break;
            }
        }
    }
    
    // Push to redo stack
    m_redoStack.push(entry);
    
    // Update active stack based on what's left on undo stack
    if (m_undoStack.isEmpty()) {
        m_activeStack = ActiveStack::None;
    } else {
        m_activeStack = m_undoStack.top().isFixed ? ActiveStack::Fixed : ActiveStack::Moving;
    }
    
    // Rebuild pairs and update view
    rebuildPairs();
    
    emit pointRemoved(entry.pairIndex, entry.isFixed);
    emit undoRedoStateChanged();
    
    return true;
}

bool TiePointModel::redoLastPoint()
{
    if (m_redoStack.isEmpty())
        return false;
    
    UndoEntry entry = m_redoStack.pop();
    
    // Add the point back to the appropriate list
    if (entry.isFixed) {
        m_fixedPoints.append(PointEntry(entry.pairIndex, entry.position));
    } else {
        m_movingPoints.append(PointEntry(entry.pairIndex, entry.position));
    }
    
    // Push back to undo stack
    m_undoStack.push(entry);
    
    // Update active stack
    m_activeStack = entry.isFixed ? ActiveStack::Fixed : ActiveStack::Moving;
    
    // Rebuild pairs and update view
    rebuildPairs();
    
    emit pointAdded(entry.pairIndex, entry.isFixed);
    emit undoRedoStateChanged();
    
    return true;
}

// ============================================================================
// Query Methods
// ============================================================================

TiePointPair TiePointModel::getPair(int index) const
{
    if (index < 0 || index >= m_pairs.count())
        return TiePointPair();
    return m_pairs.at(index);
}

QList<TiePointPair> TiePointModel::getAllPairs() const
{
    return m_pairs;
}

QList<TiePointPair> TiePointModel::getCompletePairs() const
{
    QList<TiePointPair> complete;
    for (const TiePointPair &pair : m_pairs) {
        if (pair.isComplete()) {
            complete.append(pair);
        }
    }
    return complete;
}

int TiePointModel::pairCount() const
{
    return m_pairs.count();
}

int TiePointModel::completePairCount() const
{
    int count = 0;
    for (const TiePointPair &pair : m_pairs) {
        if (pair.isComplete()) {
            ++count;
        }
    }
    return count;
}

bool TiePointModel::canUndo() const
{
    return !m_undoStack.isEmpty();
}

bool TiePointModel::canRedo() const
{
    return !m_redoStack.isEmpty();
}

bool TiePointModel::hasBothPoints(int pairIndex) const
{
    for (const TiePointPair &pair : m_pairs) {
        if (pair.index == pairIndex) {
            return pair.isComplete();
        }
    }
    return false;
}

// ============================================================================
// Legacy Compatibility
// ============================================================================

void TiePointModel::addTiePoint(const QPointF &fixed, const QPointF &moving)
{
    int pairIndex = getNextPairIndex();
    
    m_fixedPoints.append(PointEntry(pairIndex, fixed));
    m_movingPoints.append(PointEntry(pairIndex, moving));
    
    // Clear redo stack
    m_redoStack.clear();
    
    // For legacy API, we don't add individual undo entries
    // Just rebuild pairs
    rebuildPairs();
    
    emit pairCompleted(pairIndex);
}

void TiePointModel::removeTiePoint(int index)
{
    if (index < 0 || index >= m_pairs.count())
        return;

    int pairIndex = m_pairs.at(index).index;
    
    // Remove from fixed points
    for (int i = m_fixedPoints.size() - 1; i >= 0; --i) {
        if (m_fixedPoints[i].pairIndex == pairIndex) {
            m_fixedPoints.removeAt(i);
        }
    }
    
    // Remove from moving points
    for (int i = m_movingPoints.size() - 1; i >= 0; --i) {
        if (m_movingPoints[i].pairIndex == pairIndex) {
            m_movingPoints.removeAt(i);
        }
    }
    
    // Remove from undo stack entries with this pair index
    QStack<UndoEntry> tempStack;
    while (!m_undoStack.isEmpty()) {
        UndoEntry entry = m_undoStack.pop();
        if (entry.pairIndex != pairIndex) {
            tempStack.push(entry);
        }
    }
    while (!tempStack.isEmpty()) {
        m_undoStack.push(tempStack.pop());
    }
    
    rebuildPairs();
    emit pointRemoved(pairIndex, true);
    emit undoRedoStateChanged();
}

void TiePointModel::insertTiePoint(int index, const QPointF &fixed, const QPointF &moving)
{
    // For legacy compatibility, we need to insert at a specific position
    // Use the next available pair index
    int pairIndex = getNextPairIndex();
    
    // Add to storage
    m_fixedPoints.append(PointEntry(pairIndex, fixed));
    m_movingPoints.append(PointEntry(pairIndex, moving));
    
    // Rebuild pairs
    rebuildPairs();
    
    emit pairCompleted(pairIndex);
}

void TiePointModel::clearAll()
{
    if (m_fixedPoints.isEmpty() && m_movingPoints.isEmpty())
        return;

    beginResetModel();
    m_fixedPoints.clear();
    m_movingPoints.clear();
    m_undoStack.clear();
    m_redoStack.clear();
    m_pairs.clear();
    m_activeStack = ActiveStack::None;
    endResetModel();
    
    emit modelCleared();
    emit undoRedoStateChanged();
}

TiePoint TiePointModel::getTiePoint(int index) const
{
    if (index < 0 || index >= m_pairs.count())
        return TiePoint();
    
    const TiePointPair &pair = m_pairs.at(index);
    TiePoint tp;
    if (pair.hasFixed())
        tp.fixed = *pair.fixed;
    if (pair.hasMoving())
        tp.moving = *pair.moving;
    return tp;
}

QList<TiePoint> TiePointModel::getAllTiePoints() const
{
    QList<TiePoint> result;
    for (const TiePointPair &pair : m_pairs) {
        if (pair.isComplete()) {
            result.append(TiePoint(*pair.fixed, *pair.moving));
        }
    }
    return result;
}

void TiePointModel::updateFixedPoint(int index, const QPointF &point)
{
    if (index < 0 || index >= m_pairs.count())
        return;

    int pairIndex = m_pairs.at(index).index;
    int idx = findFixedPointIndex(pairIndex);
    if (idx >= 0) {
        m_fixedPoints[idx].position = point;
        rebuildPairs();
        
        QModelIndex topLeft = createIndex(index, ColFixedX);
        QModelIndex bottomRight = createIndex(index, ColFixedY);
        emit dataChanged(topLeft, bottomRight);
    }
}

void TiePointModel::updateMovingPoint(int index, const QPointF &point)
{
    if (index < 0 || index >= m_pairs.count())
        return;

    int pairIndex = m_pairs.at(index).index;
    int idx = findMovingPointIndex(pairIndex);
    if (idx >= 0) {
        m_movingPoints[idx].position = point;
        rebuildPairs();
        
        QModelIndex topLeft = createIndex(index, ColMovingX);
        QModelIndex bottomRight = createIndex(index, ColMovingY);
        emit dataChanged(topLeft, bottomRight);
    }
}

// ============================================================================
// Private Helpers
// ============================================================================

void TiePointModel::rebuildPairs()
{
    beginResetModel();
    
    // Collect all unique pair indices
    QSet<int> indices;
    for (const PointEntry &entry : m_fixedPoints) {
        indices.insert(entry.pairIndex);
    }
    for (const PointEntry &entry : m_movingPoints) {
        indices.insert(entry.pairIndex);
    }
    
    // Sort indices
    QList<int> sortedIndices = indices.values();
    std::sort(sortedIndices.begin(), sortedIndices.end());
    
    // Build pairs
    m_pairs.clear();
    for (int idx : sortedIndices) {
        TiePointPair pair(idx);
        
        // Find fixed point
        for (const PointEntry &entry : m_fixedPoints) {
            if (entry.pairIndex == idx) {
                pair.fixed = entry.position;
                break;
            }
        }
        
        // Find moving point
        for (const PointEntry &entry : m_movingPoints) {
            if (entry.pairIndex == idx) {
                pair.moving = entry.position;
                break;
            }
        }
        
        m_pairs.append(pair);
    }
    
    endResetModel();
}

int TiePointModel::getNextPairIndex() const
{
    int maxIndex = -1;
    for (const PointEntry &entry : m_fixedPoints) {
        maxIndex = qMax(maxIndex, entry.pairIndex);
    }
    for (const PointEntry &entry : m_movingPoints) {
        maxIndex = qMax(maxIndex, entry.pairIndex);
    }
    return maxIndex + 1;
}

int TiePointModel::findFixedPointIndex(int pairIndex) const
{
    for (int i = 0; i < m_fixedPoints.size(); ++i) {
        if (m_fixedPoints[i].pairIndex == pairIndex) {
            return i;
        }
    }
    return -1;
}

int TiePointModel::findMovingPointIndex(int pairIndex) const
{
    for (int i = 0; i < m_movingPoints.size(); ++i) {
        if (m_movingPoints[i].pairIndex == pairIndex) {
            return i;
        }
    }
    return -1;
}