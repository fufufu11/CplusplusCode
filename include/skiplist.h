#pragma once
#include <vector>
#include <memory>
#include <random>
#include <optional>
/**
 * @brief 跳表节点结构体
 * @tparam K 键类型
 * @tparam V 值类型 
 */
template <typename K, typename V>
struct Node {
    K key;      // 存储的键
    V value;    // 存储的值

    /**
     * @brief 前进指针数组 (Forward List)
     * forward[i] 存储的是该节点在第 i 层的下一个节点的指针
     */
    std::vector<Node<K, V>*> forward;

    /**
     * @brief Node 构造函数
     * @param k 键
     * @param v 值
     * @param level 节点的高度（层数）
     */
    Node(K k, V v, int level)
        : key(k), value(v), forward(level, nullptr) {}
};

/**
 * @brief 跳表类模板
 */
template <typename K, typename V>
class SkipList {
private:
    int max_level;      // 跳表允许的最大层数
    int current_level;  // 当前跳表实际存在的最高层数
    Node<K, V>* head;   // 跳表的头节点（哨兵节点）
    float p;            // 节点层数晋升的概率因子

    std::vector<std::unique_ptr<Node<K, V>>> nodes_storage;
    std::mt19937 rng;
    std::uniform_real_distribution<float> dist;

    /**
     * @brief 随机生成节点的层数
     * 
     * 使用几何分布逻辑：每一层晋升到更高一层的概率为 p。
     * 
     * @return int 生成的随机层数，范围在 [1, max_level]
     */
    int random_level() {
        int level = 1;
        while (dist(rng) < p && level < max_level) {
            level++;
        }
        return level;
    }

public:
    /**
     * @brief SkipList 构造函数
     * @param max_lvl 最大允许层数
     * @param prob 晋升概率因子，默认 0.5
     */
    SkipList(int max_lvl, float prob = 0.5f)
        : max_level(max_lvl),
          current_level(1),
          p(prob),
          rng(std::random_device{}()),
          dist(0.0f, 1.0f) {
        // 创建哨兵头节点，其高度为最大层数
        auto head_node = std::make_unique<Node<K, V>>(K(), V(), max_level);
        head = head_node.get();
        nodes_storage.push_back(std::move(head_node));
    }

    /**
     * @brief 析构函数，负责释放跳表中所有节点的内存
     */
    ~SkipList() = default;

    SkipList(const SkipList&) = delete;
    SkipList& operator=(const SkipList&) = delete;

    SkipList(SkipList&& other) noexcept
        : max_level(other.max_level)
        , current_level(other.current_level)
        , head(other.head)
        , p(other.p)
        , nodes_storage(std::move(other.nodes_storage))
        , rng(std::move(other.rng))
        , dist(std::move(other.dist)) {
        other.head = nullptr;
        other.current_level = 1;
    }

    SkipList& operator=(SkipList&& other) noexcept {
        if (this != &other) {
            max_level = other.max_level;
            current_level = other.current_level;
            head = other.head;
            p = other.p;
            nodes_storage = std::move(other.nodes_storage);
            rng = std::move(other.rng);
            dist = std::move(other.dist);
            other.head = nullptr;
            other.current_level = 1;
        }
        return *this;
    }

    void clear() {
        nodes_storage.clear();
        auto head_node = std::make_unique<Node<K, V>>(K(), V(), max_level);
        head = head_node.get();
        nodes_storage.push_back(std::move(head_node));
        current_level = 1;
    }

    /**
     * @brief 插入或更新一个键值对
     * 
     * @param key 待插入的键
     * @param value 待插入的值
     * @return true 插入成功
     * @return false 插入失败
     */
    bool insert(K key, V value);

    /**
     * @brief 根据键查找对应的值
     * 
     * @param key 待查找的键
     * @return std::optional<V> 如果找到则返回包含值的 optional，否则返回 std::nullopt
     */
    std::optional<V> search(K key);

    /**
     * @brief 根据键删除节点
     * 
     * @param key 待删除的键
     * @return true 删除成功
     * @return false 键不存在，删除失败
     */
    bool remove(K key);

    /**
     * @brief 获取跳表中的节点数量
     *
     * @return size_t 节点数量（不含哨兵头节点）
     */
    size_t size() const { return nodes_storage.size() - 1; }

    /**
     * @brief 检查跳表是否为空
     *
     * @return true 为空
     * @return false 不为空
     */
    bool empty() const { return size() == 0; }

    /**
     * @brief 前向声明迭代器类
     */
    class Iterator;

    /**
     * @brief 获取指向第一个元素的迭代器
     *
     * @return Iterator 指向最小 key 的迭代器
     */
    Iterator begin() const { return Iterator(head->forward[0]); }

    /**
     * @brief 获取尾后迭代器
     *
     * @return Iterator 尾后迭代器（nullptr）
     */
    Iterator end() const { return Iterator(nullptr); }
};

template <typename K, typename V>
bool SkipList<K, V>::insert(K key, V value) {
    // 1. 初始化 update 数组，用于记录每一层中目标 key 的前驱节点
    std::vector<Node<K, V>*> update(max_level, nullptr);
    Node<K, V>* current = head;

    // 2. 从最高层向下寻找插入位置
    for(int i = current_level - 1; i >= 0; i--){
        // 在当前层级，向右移动，直到下一个节点的 key 大于等于目标 key
        while(current->forward[i] && current->forward[i]->key < key){
            current = current->forward[i];
        }
        // 记录当前层级的前驱节点
        update[i] = current;
    }

    // 3. 检查是否已存在该 Key
    // current 现在指向 Level 0 的前驱，检查它的下一个节点
    current = current->forward[0];
    if(current && current->key == key){
        current->value = value; // Key 存在，更新 Value
        return true;
    }

    // 4. 生成新节点的随机层数
    int new_level = random_level();

    // 5. 如果新层数超过当前跳表的最大层数，需要更新 update 数组
    // 将超出的层数的前驱指向 head
    if(new_level > current_level){
        for(int i = current_level; i < new_level; i++){
            update[i] = head;
        }
        current_level = new_level;
    }

    // 6. 创建新节点
    // 使用 make_unique 创建节点，确保异常安全
    auto new_node = std::make_unique<Node<K,V>>(key, value, new_level);
    Node<K, V>* ptr = new_node.get(); // 获取裸指针用于链表操作

    // 7. 逐层调整指针，将新节点插入链表
    for(int i = 0; i < new_level; i++){
        // 新节点指向前驱的下一个节点
        ptr->forward[i] = update[i]->forward[i];
        // 前驱指向新节点
        update[i]->forward[i] = ptr;
    }

    // 8. 将新节点的所有权移交给 nodes_storage 管理，避免内存泄漏
    nodes_storage.push_back(std::move(new_node));
    return true;
}

template <typename K, typename V>
std::optional<V> SkipList<K, V>::search(K key){
    // 从头结点（哨兵）出发。head 不存有效数据，只用于统一边界处理
    Node<K, V>* current = head;

    // 从当前跳表“实际最高层”开始逐层下沉查找：
    // 在每一层都尽量向右走，直到右侧节点的 key >= 目标 key（或到达 NULL）为止
    for(int i = current_level - 1; i >= 0; i--) {
        // forward[i] 相当于这一层的 next 指针
        // 循环结束时，current 停在该层“严格小于 key 的最后一个节点”（即前驱节点）
        while(current->forward[i] && current->forward[i]->key < key) {
            current = current->forward[i];
        }
    }

    // 走完所有层后，current 停在 Level 0 的前驱节点上
    // 候选命中节点是 Level 0 的下一个节点
    current = current->forward[0];
    if(current && current->key == key) {
        // 命中：返回一个“有值”的 optional
        return current->value;
    }
    // 未命中：返回一个“空”的 optional
    return std::nullopt;
}

template <typename K, typename V>
bool SkipList<K, V>::remove(K key){
    /**
     * @brief 删除指定 key 对应的节点（当前阶段采用物理删除）
     *
     * 删除流程与 insert 的“寻找前驱节点”阶段同构：
     * 1) 用 update[] 记录每一层中目标 key 的前驱节点；
     * 2) 在第 0 层确认目标节点是否存在；
     * 3) 逐层断链（把前驱的 forward 指向目标的后继）；
     * 4) 必要时收缩 current_level；
     * 5) 从 nodes_storage 中擦除该节点以回收内存。
     */

    // update[i] 记录第 i 层中“目标 key 的前驱节点”
    std::vector<Node<K, V>*> update(max_level, nullptr);
    Node<K, V>* current = head;

    // 从最高层向下寻找：每一层都尽量向右走，直到下一个节点的 key >= 目标 key
    for(int i = current_level - 1; i >= 0; i--) {
        while(current->forward[i] && current->forward[i]->key < key) {
            current = current->forward[i];
        }
        // 记录该层的前驱节点，供后续断链使用
        update[i] = current;
    }

    // 走完所有层后，current 与 update[0] 等价：都是 Level 0 的前驱节点
    // 候选目标节点位于 Level 0 的下一个位置
    Node<K, V>* target = current->forward[0];
    if(!target || target->key != key) {
        // key 不存在：不修改结构
        return false;
    }

    // 逐层断链：如果该层确实指向 target，才把指针改为跨过 target
    // 注意：target 的高度可能小于 current_level，更高层可能根本不包含 target
    for(int i = 0; i < current_level; i++) {
        if(update[i]->forward[i] == target) {
            update[i]->forward[i] = target->forward[i];
        }
    }

    // 删除后如果最高层为空（只剩 head），则收缩 current_level，避免后续遍历空层
    while(current_level > 1 && head->forward[current_level - 1] == nullptr) {
        --current_level;
    }

    // 断链仅保证"逻辑不可达"；要回收内存，需要把对应节点从 nodes_storage 中擦除
    for(auto it = nodes_storage.begin(); it != nodes_storage.end(); ++it) {
        if(it->get() == target) {
            nodes_storage.erase(it);
            break;
        }
    }
    return true;
}

/**
 * @brief SkipList 迭代器类
 *
 * 提供顺序遍历跳表中所有 KV 对的能力。
 * 迭代器只遍历 Level 0（包含所有节点的完整链表）。
 *
 * @tparam K 键类型
 * @tparam V 值类型
 */
template <typename K, typename V>
class SkipList<K, V>::Iterator {
public:
    /**
     * @brief 构造函数
     *
     * @param node 起始节点指针
     */
    explicit Iterator(Node<K, V>* node) : current_(node) {}

    /**
     * @brief 解引用运算符：获取当前节点的键
     *
     * @return const K& 当前键的常量引用
     */
    const K& key() const { return current_->key; }

    /**
     * @brief 解引用运算符：获取当前节点的值
     *
     * @return const V& 当前值的常量引用
     */
    const V& value() const { return current_->value; }

    /**
     * @brief 前置递增运算符：移动到下一个节点
     *
     * @return Iterator& 移动后的迭代器引用
     */
    Iterator& operator++() {
        if (current_) {
            current_ = current_->forward[0];
        }
        return *this;
    }

    /**
     * @brief 后置递增运算符：移动到下一个节点
     *
     * @return Iterator 移动前的迭代器副本
     */
    Iterator operator++(int) {
        Iterator tmp = *this;
        ++(*this);
        return tmp;
    }

    /**
     * @brief 相等比较运算符
     */
    bool operator==(const Iterator& other) const {
        return current_ == other.current_;
    }

    /**
     * @brief 不相等比较运算符
     */
    bool operator!=(const Iterator& other) const {
        return current_ != other.current_;
    }

private:
    Node<K, V>* current_;
};
