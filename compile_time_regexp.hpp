// Copyright (c) Chao Wang, hit9@icloud.com, 2023.
// Compile time regular expression engine.
// vim: foldmethod=marker foldenable

#ifndef __COMPILETIME_REGEXP_H__
#define __COMPILETIME_REGEXP_H__

#include <algorithm>         // for std::copy_n, std::fill, std::sort, std::max
#include <cstdlib>           // for std::size_t
#include <initializer_list>  // for std::initializer_list
#include <string>            // for std::string
#include <string_view>       // for std::string_view
#include <tuple>             // for std::tuple, std::tie
#include <vector>            // for std::vector

#ifdef DEBUG
#include "ctp/ctp.hpp"
#endif

// Compile time regular expression engine.
namespace ctre {
namespace _ {

// Hash - Compile time hash functions. {{{

// FNV32 implementation.
constexpr uint32_t fnv32(const char *s, std::size_t n) {
  uint32_t h = 0x811c9dc5;
  for (auto i = 0; i < n; i++) {
    h *= 16777619;
    h ^= s[i];
  }
  return h;
}

// hash<T> functions.
template <typename T>
struct hash {
  constexpr uint32_t operator()(T v) const;
};

template <>
struct hash<char> {
  constexpr uint32_t operator()(char v) const {
    char s[1] = {v};
    return fnv32(s, 1);
  };
};

template <>
struct hash<uint32_t> {
  constexpr uint32_t operator()(uint32_t v) const {
    char s[4] = {0};
    s[3] = (v >> 24) & 0xff;
    s[2] = (v >> 16) & 0xff;
    s[1] = (v >> 8) & 0xff;
    s[0] = v & 0xff;
    return fnv32(s, 4);
  };
};

template <>
struct hash<std::vector<uint32_t>> {
  constexpr uint32_t operator()(const std::vector<uint32_t> &v) const {
    uint32_t h = 0x811c9dc5;
    for (auto i = 0; i < v.size(); i++) {
      h *= 16777619;
      h ^= v[i];
    }
    return h;
  };
};
// hash functions. }}}

// Stack -- compile time stack. {{{
template <typename T>
class stack {
 private:
  std::vector<T> a;

 public:
  constexpr stack(){};
  constexpr std::size_t size() const { return a.size(); }
  constexpr bool empty() const { return a.empty(); }
  // Returns the reference to stack top.
  constexpr T &top() { return a.back(); }
  // Push a copy of given element onto the stack.
  constexpr void push(T v) { a.push_back(v); }
  // Pop stack top element and returns a copy.
  constexpr T pop() {
    // copy last.
    auto v = a.back();
    // pop and destruct.
    a.pop_back();
    return v;
  }
};
// stack. }}}

// Map - Compile time unordered map. {{{
// Open addressing based.
// K is the type of key.
// V is the type of value.
// H is the hasher class.
template <typename K, typename V, typename H = hash<K>>
class map {
 private:
  // Hashtable slot.
  struct slot {
    K k;
    V v;
    bool used = false;
  };
  // Slots buffer.
  slot *b = nullptr;
  // Capacity.
  std::size_t cap = 0;
  // Number of elements.
  std::size_t n = 0;

  // Hasher.
  static constexpr auto h = H{};

  // Returns the position via haser h.
  static constexpr std::size_t mod(const K &k, std::size_t i, std::size_t cap) {
    return (h(k) + i) % cap;
  }

  // Resize capacity.
  constexpr void resize() {
    auto new_cap = std::max<std::size_t>(cap * 2, 7);
    auto ob = b;
    b = new slot[new_cap]{};
    for (auto i = 0; i < cap; i++) {
      auto &sl = ob[i];
      if (sl.used) {
        for (auto j = 0; j < new_cap; j++) {
          auto &sl1 = b[j];
          if (!sl1.used) {
            sl1.k = sl.k;
            sl1.v = sl.v;
            sl1.used = true;
            break;
          }
        }
      }
    }
    delete[] ob;
    cap = new_cap;
  }

 public:
  constexpr map() {}
  constexpr ~map() { delete[] b; }
  constexpr map(const map &o) { assign(o); }
  constexpr std::size_t size() const { return n; }
  constexpr bool empty() const { return n == 0; }
  constexpr void assign(const map &o) {
    if (this != &o) {  // if not assigning itself.
      delete[] b;
      b = nullptr;
      n = o.n;
      cap = o.cap;
      if (cap > 0) {
        b = new slot[cap]{};
        std::copy_n(o.b, cap, b);
      }
    }
  }

  constexpr map &operator=(const map &o) {
    assign(o);
    return *this;
  }
  // Equals if:
  // 1. Equals in size.
  // 2. One's elements are all in another.
  constexpr bool operator==(const map &o) const {
    if (n != o.n) return false;
    for (auto p : o) {
      auto &[k, v] = p;
      auto ptr = getp(k);
      if (ptr == nullptr) return false;  // not found
      if (*ptr != v) return false;       // not equal.
    }
    return true;
  }
  constexpr bool operator!=(const map &o) const { return !(*this == o); }
  // Subscript operator to get value from this map.
  // Sets an empty value if key does not exist.
  constexpr V &operator[](const K &k) {
    auto p = getp(k);
    if (p != nullptr) return *p;
    // Not found, sets one.
    set(k, V{});
    return get(k);
  }

  // Sets a pair of key, value to table (copy).
  constexpr bool set(const K &k, const V &v) {
    // Resize if capacity is too small.
    if (cap < (n + 1) || cap * 0.8 < n + 1) resize();

    for (auto i = 0; i < cap; i++) {
      auto p = mod(k, i, cap);
      auto &node = b[p];
      if (!node.used || node.k == k) {  // insert or update
        node.k = k;
        node.v = v;
        if (!node.used) {  // insert
          n++;
          node.used = true;
        }
        return true;
      }
    }
    return false;
  };

  // Gets a value by key.
  // Throws if the key is not found.
  constexpr V &get(const K &k) const {
    auto p = getp(k);
    if (p == nullptr) throw "not found";
    return *p;
  }

  // Gets a pointer of value by key.
  // Returns nullptr if key is not found.
  constexpr V *getp(const K &k) const {
    if (empty()) return nullptr;
    for (auto i = 0; i < cap; i++) {
      auto p = mod(k, i, cap);
      auto &node = b[p];
      if (node.used && node.k == k) return &node.v;
    }
    return nullptr;
  }

  // Has checks if a key exist in this table.
  constexpr bool has(const K &k) const {
    auto p = getp(k);
    if (p == nullptr) return false;
    return true;
  }

  // Pop a value by key.
  constexpr void pop(const K &k) {
    if (empty()) return;
    for (auto i = 0; i < cap; i++) {
      auto p = mod(k, i, cap);
      auto &node = b[p];
      if (node.used && node.k == k) {
        node.used = false;
        n--;
      }
    }
  }

  // Minimal iterator for map.
  class iterator {
   private:
    slot *p = nullptr;
    slot *end = nullptr;

   public:
    constexpr iterator(){};
    constexpr iterator(slot *q, slot *e) : p(q), end(e) {
      // Seeks to the first used slot.
      while (p != nullptr && p != end && !p->used) p++;
    }
    // Returns a tuple of references to k and v.
    constexpr std::tuple<K &, V &> operator*() const {
      return std::tie(p->k, p->v);
    }
    // Prefix ++iterator
    constexpr iterator &operator++() {
      // Seeks to next used slot.
      do {
        p++;
      } while (p != end && !p->used);
      return *this;
    }
    constexpr bool operator!=(const iterator &o) { return p != o.p; };
  };

  // Returns the iterator at begin.
  constexpr iterator begin() const {
    if (!n) return iterator();
    return iterator(b, b + cap);
  }

  // Returns the iterator at end.
  constexpr iterator end() const {
    if (!n) return iterator();
    auto end = b + cap;
    return iterator(end, end);
  }
};
// map }}}

// Set - Compile time set based on map. {{{
template <typename T, typename H = hash<T>>
class set {
 private:
  using map_t = map<T, bool, H>;
  using map_iterator_t = typename map<T, bool, H>::iterator;
  // internal map<T, bool>
  map_t m;

 public:
  constexpr set() {}
  constexpr set(std::initializer_list<T> vs) {
    for (auto &v : vs) add(v);
  }
  constexpr set(const set &o) { m = o.m; }
  constexpr set &operator=(const set &o) {
    if (this != &o) m = o.m;
    return *this;
  }
  constexpr std::size_t size() const { return m.size(); }
  constexpr bool empty() const { return m.empty(); }
  constexpr bool operator==(const set &o) const { return m == o.m; }
  constexpr bool operator!=(const set &o) const { return m != o.m; }
  constexpr bool add(const T &v) { return m[v] = true; }
  constexpr bool has(const T &v) const { return m.has(v); }
  constexpr void pop(T v) { m.pop(v); }
  constexpr void merge(const set &o) {
    for (auto &v : o) add(v);
  }

  class iterator {
   private:
    map_iterator_t it;

   public:
    constexpr iterator(map_iterator_t it) : it(it){};
    constexpr T &operator*() const { return std::get<0>(*it); }
    constexpr iterator &operator++() {
      ++it;
      return *this;
    }
    constexpr bool operator!=(const iterator &o) { return it != o.it; };
  };
  constexpr iterator begin() const { return iterator(m.begin()); }
  constexpr iterator end() const { return iterator(m.end()); }
};
// set }}}

// Queue - A compile-time unique queue based on set. {{{
template <typename T, typename H = hash<T>>
class unique_queue {
 private:
  struct Node {
    T v;
    Node *next = nullptr;
    Node *prev = nullptr;
    constexpr Node(){};
    constexpr Node(T v, Node *prev) : v(v), next(nullptr), prev(prev){};
  };

 public:
  Node *head = nullptr;  // virtual head
  Node *last = nullptr;
  set<T, H> s;
  constexpr unique_queue() {
    head = new Node();
    last = head;
  };
  constexpr ~unique_queue() noexcept {
    while (!empty()) pop();
    delete head;
  }
  constexpr std::size_t size() const { return s.size(); }
  constexpr bool empty() const { return s.empty(); }
  constexpr bool has(const T &v) const { return s.has(v); }
  // Push an element to this queue, returns false if already exist.
  constexpr bool push(const T &v) {
    if (s.has(v)) return false;
    auto new_node = new Node(v, last);
    last->next = new_node;
    last = new_node;
    s.add(v);
    return true;
  }
  // Pops the front data.
  constexpr T pop() {
    if (empty()) throw "empty q";
    auto front = head->next;  // to delete
    if (front->next != nullptr) front->next->prev = head;
    head->next = front->next;
    // When there's only one node, the final last should be head itself.
    if (front == last) last = head;
    // Pops the data.
    auto v = front->v;
    s.pop(v);
    delete front;
    return v;
  }
  // Front returns the front.
  constexpr T &front() {
    if (empty()) throw "empty q";
    return head->next->v;
  }
};
// queue }}}

// Fixed size string. {{{
template <std::size_t N>
class fixed_string {
 public:
  char a[N];
  constexpr fixed_string(const char (&s)[N]) { std::copy_n(s, N, a); }
  constexpr std::size_t size() const { return N; }
  constexpr char operator[](int x) const { return a[x]; }
};
// end fixed_string }}}

// {{{ Helpers
// Regexp character.
using C = char;

// Epsilon character.
static constexpr C EPSILON = '\0';

// Supported Operators
enum Op {
  OP_CONCAT = '&',
  OP_UNION = '|',
  OP_CLOSURE = '*',
  OP_LEFT_PAIR = '(',
  OP_RIGHT_PAIR = ')',
  OP_PLUS = '+',
  OP_OPTIONAL = '?',
  OP_RANGE_START = '[',
  OP_RANGE_END = ']',
  OP_RANGE_TO = '-',
};

// Is given character is a valid calculation operator.
constexpr static bool IsCalculationOperator(C c) {
  switch (c) {
    case OP_CONCAT:
      return true;
    case OP_UNION:
      return true;
    case OP_CLOSURE:
      return true;
    case OP_PLUS:
      return true;
    case OP_OPTIONAL:
      return true;
    default:
      return false;
  }
}

// Get priority of given operator.
constexpr static int GetOperatorPriority(Op op) {
  switch (op) {
    case OP_CONCAT:
      return 1;
    case OP_UNION:
      return 1;
    case OP_CLOSURE:
      return 2;
    case OP_PLUS:
      return 2;
    case OP_OPTIONAL:
      return 2;
    default:
      return 0;
  };
}

// Is given character has effects on right hand.
constexpr static bool IsRightActingOperator(C c) {
  switch (c) {
    case OP_CONCAT:
      [[fallthrough]];
    case OP_UNION:
      [[fallthrough]];
    case OP_LEFT_PAIR:
      return true;
    default:
      return false;
  }
}

// Is given character able to insert a concat symbol on the left?
constexpr static bool IsAbleInsertConcat(C c) {
  switch (c) {
    case OP_CONCAT:
      [[fallthrough]];
    case OP_UNION:
      [[fallthrough]];
    case OP_CLOSURE:
      [[fallthrough]];
    case OP_RIGHT_PAIR:
      [[fallthrough]];
    case OP_PLUS:
      [[fallthrough]];
    case OP_OPTIONAL:
      [[fallthrough]];
    case OP_RANGE_END:
      [[fallthrough]];
    case OP_RANGE_TO:
      return false;
    default:
      // OP_LEFT_PAIR, normal character symbols should be ok.
      return true;
  }
}
// Helpers }}}

// State - Base state class. {{{
class State {
 protected:
  uint32_t id;  // starts from 1.
  bool is_end;

 public:
  constexpr State() : id(0), is_end(false){};
  constexpr State(uint32_t id, bool is_end) : id(id), is_end(is_end){};
  constexpr uint32_t Id() const { return id; };
  constexpr bool IsEnd() const { return is_end; };
  friend constexpr bool operator==(const State &a, const State &b) {
    return a.Id() == b.Id();
  }
  friend constexpr bool operator!=(const State &a, const State &b) {
    return a.Id() != b.Id();
  }
};

template <>
struct hash<State *> {
  constexpr uint32_t operator()(State *st) const {
    return hash<uint32_t>{}(st->Id());
  };
};
// State }}}

// NfaParser - NfaState, Nfa & Parse Nfa from regular expression pattern. {{{
class NfaState : public State {
 public:
  // Set of NfaStates pointers.
  using PtrSet = set<NfaState *, hash<State *>>;

  // Transition table: character => NfaState poniters.
  using Table = map<C, PtrSet>;

  constexpr NfaState() : State(){};
  constexpr NfaState(int id, bool is_end) : State(id, is_end){};
  constexpr Table &Transitions() { return transitions; };
  // Add a transition via character c to given state.
  constexpr void AddTransition(C c, NfaState *to) {
    transitions[c].add(to);
    if (is_end) is_end = false;
  }
  // Is given character c is acceptable by this state?
  constexpr bool AcceptC(C c) const { return transitions.has(c); }
  // Returns the target states reaches through given character.
  constexpr PtrSet &Nexts(C c) { return transitions.get(c); };

 private:
  Table transitions;
};

class Nfa {
 public:
  NfaState *start = nullptr;
  NfaState *end = nullptr;
  std::size_t size = 0;
  constexpr Nfa(){};
  constexpr Nfa(NfaState *start, NfaState *end, std::size_t size)
      : start(start), end(end), size(size){};
};

// NfaParser parses Nfa from regular expression.
// It's stateful, the parsed Nfa's lifetime depends on this parser's.
class NfaParser {
 private:
  // Range<start character, end character>
  using range = std::tuple<C, C>;
  // Stack of Nfa pointers.
  stack<Nfa *> nfa_stack;
  // Stack of operators.
  stack<Op> op_stack;
  // Vector of allocated nfa states pointers.
  std::vector<NfaState *> states;

  // Creates a new state.
  constexpr NfaState *NewState(bool is_end) {
    auto id = states.size() + 1;
    auto p = new NfaState(id, is_end);
    states.push_back(p);
    return p;
  }

  // Creates a nfa from a single symbol.
  //       c
  // start -> end
  constexpr Nfa *NewNfaFromSymbol(C c) {
    auto start = NewState(false);
    auto end = NewState(true);
    start->AddTransition(c, end);
    return new Nfa(start, end, 2);
  }

  // Creates a nfa from a set of symbols.
  constexpr Nfa *NewNfaFromSymbols(set<C> &chs) {
    auto start = NewState(false);
    auto end = NewState(true);
    if (!chs.empty()) {
      for (auto &c : chs) {
        start->AddTransition(c, end);
      }
    } else {
      start->AddTransition(EPSILON, end);
    }
    return new Nfa(start, end, 2);
  }

  // Create a nfa from a set of ranges.
  constexpr Nfa *NewNfaFromRanges(std::vector<range> &ranges) {
    set<C> chs;
    for (auto &range : ranges) {
      auto [start, end] = range;
      for (auto x = start; x <= end; x++) chs.add(x);
    }
    return NewNfaFromSymbols(chs);
  }

  // Concats two nfa a and b, creates a new nfa to the stack.
  // a: 1 -> 2
  // b: 3 -> 4
  // ab: 1 -> 2 -> 3 -> 4
  constexpr Nfa *NewNfaFromConcat(Nfa *a, Nfa *b) {
    a->end->AddTransition(EPSILON, b->start);
    return new Nfa(a->start, b->end, a->size + b->size);
  }

  // Union
  // a: 1 -> 2
  // b: 3 -> 4
  // a|b:
  //      e         e
  //     +-> 1 -> 2 -+
  //  5->+           +-> 6
  //     +-> 3 -> 4 -+
  //      e         e
  constexpr Nfa *NewNfaFromUnion(Nfa *a, Nfa *b) {
    auto start = NewState(false);             // 5
    auto end = NewState(true);                // 6
    start->AddTransition(EPSILON, a->start);  // 5->1
    start->AddTransition(EPSILON, b->start);  // 5->3
    a->end->AddTransition(EPSILON, end);      // 2->6
    b->end->AddTransition(EPSILON, end);      // 4->6
    return new Nfa(start, end, a->size + b->size + 2);
  }

  // Closure
  // a: 1->2
  // a*:
  //         e
  //       +----+
  //    e  V    | e
  //  3 -> 1 -> 2 -> 4
  //  |              ^
  //  +--------------+
  //          e
  constexpr Nfa *NewNfaFromClosure(Nfa *a) {
    auto start = NewState(false);              // 3
    auto end = NewState(true);                 // 4
    a->end->AddTransition(EPSILON, a->start);  // 2->1
    start->AddTransition(EPSILON, a->start);   // 3->1
    a->end->AddTransition(EPSILON, end);       // 2->4
    start->AddTransition(EPSILON, end);        // 3->4
    return new Nfa(start, end, a->size + 2);
  }

  // a+ that is aa*
  constexpr Nfa *NewNfaFromPlus(Nfa *a) {
    auto p = NewNfaFromClosure(a);
    auto q = NewNfaFromConcat(a, p);
    delete p;
    return q;
  }

  // Optional
  // a: 1->2
  // a?:
  //   e         e
  // 3 -> 1 -> 2 -> 4
  // |              ^
  // +--------------+
  //         e
  constexpr Nfa *NewNfaFromOptional(Nfa *a) {
    auto start = NewState(false);             // 3
    auto end = NewState(true);                // 4
    start->AddTransition(EPSILON, a->start);  // 3->1
    a->end->AddTransition(EPSILON, end);      // 2->4
    start->AddTransition(EPSILON, end);       // 3->4
    return new Nfa(start, end, a->size + 2);
  }

  // Calculate on the top of stacks.
  constexpr void Calc() {
    if (op_stack.empty()) return;
    // Pop an operator.
    auto op = op_stack.pop();
    switch (op) {
      case OP_CLOSURE: {
        auto a = nfa_stack.pop();
        nfa_stack.push(NewNfaFromClosure(a));
        delete a;
      }; break;
      case OP_CONCAT: {
        auto b = nfa_stack.pop();
        auto a = nfa_stack.pop();
        nfa_stack.push(NewNfaFromConcat(a, b));
        delete a;
        delete b;
      }; break;
      case OP_UNION: {
        auto b = nfa_stack.pop();
        auto a = nfa_stack.pop();
        nfa_stack.push(NewNfaFromUnion(a, b));
        delete a;
        delete b;
      }; break;
      case OP_PLUS: {
        auto a = nfa_stack.pop();
        nfa_stack.push(NewNfaFromPlus(a));
        delete a;
      } break;
      case OP_OPTIONAL: {
        auto a = nfa_stack.pop();
        nfa_stack.push(NewNfaFromOptional(a));
        delete a;
      } break;
      default:
        break;
    }
  }

  // Normalize the given regular expression s.
  // Inserts concat operators, e.g.
  // ab|c => a&b|c
  // a*c => a*&c
  // (a)b => (a)&b
  // a(ab) => a&(a&b)
  constexpr std::string Normalize(std::string_view s) {
    std::string s1;
    // Tracks wherther it's currently in a range.
    bool is_in_range = false;
    for (auto &c : s) {
      if (IsAbleInsertConcat(c) && !s1.empty() &&
          !IsRightActingOperator(s1.back()) && !is_in_range) {
        s1.push_back(OP_CONCAT);
      }
      if (c == OP_RANGE_START)
        is_in_range = true;
      else if (c == OP_RANGE_END)
        is_in_range = false;
      s1.push_back(c);
    }
    return s1;
  }

 public:
  constexpr NfaParser() noexcept {};
  constexpr ~NfaParser() noexcept {
    // Free allocated states.
    for (auto &p : states) delete p;
    // Free allocated nfas.
    while (!nfa_stack.empty()) {
      auto nfa = nfa_stack.pop();
      delete nfa;
    }
  };
  // Parse a nfa from regular expression string s.
  constexpr Nfa *Parse(std::string_view s) {
    auto s1 = Normalize(s);
    // avoid empty input.
    nfa_stack.push(NewNfaFromSymbol(EPSILON));

    auto i = 0;
    while (i < s1.size()) {
      auto x = s1[i++];
      switch (x) {
          // Calculation Operators.
        case OP_CONCAT:
          [[fallthrough]];
        case OP_UNION:
          [[fallthrough]];
        case OP_PLUS:
          [[fallthrough]];
        case OP_OPTIONAL:
          [[fallthrough]];
        case OP_CLOSURE: {
          auto op = static_cast<Op>(x);
          // Do calculation if the stack top's priority is higher.
          while (!op_stack.empty() && IsCalculationOperator(op_stack.top()) &&
                 GetOperatorPriority(op_stack.top()) >=
                     GetOperatorPriority(op)) {
            Calc();
          }
          op_stack.push(op);
        }; break;
        case OP_LEFT_PAIR:
          // Push left pair.
          op_stack.push(static_cast<Op>(x));
          break;
        case OP_RIGHT_PAIR: {
          // Do calculation on expressions inside the pair.
          while (!op_stack.empty() && op_stack.top() != OP_LEFT_PAIR) {
            Calc();
          }
          // Pop left pair.
          op_stack.pop();
        }; break;
        case OP_RANGE_START: {
          std::vector<range> ranges;
          // The start of current parsing range.
          C range_start = EPSILON;

          while (s1[i] != OP_RANGE_END && i < s1.size()) {
            x = s1[i++];
            if (x != OP_RANGE_TO) {
              if (range_start == EPSILON)
                range_start = x;
              else {
                ranges.push_back(std::make_pair(range_start, x));
                range_start = EPSILON;
              }
            }
          }
          nfa_stack.push(NewNfaFromRanges(ranges));
        } break;
        case OP_RANGE_END:
          // Skip right pair.
          break;
        case '\\':
          // Escaping
          x = s1[i++];
          // fallthrough to parse after escaping.
          [[fallthrough]];
        default:
          nfa_stack.push(NewNfaFromSymbol(x));
          break;
      }
    }
    // Clean the remaining operators in stack.
    while (!op_stack.empty()) Calc();
    return nfa_stack.top();
  }
};

// NfaParser }}}

// DfaBuilder - DfaState, Dfa & Convert Nfa to Dfa. {{{

// DfaState
class DfaState : public State {
 public:
  // Set of NfaStates.
  using Set = set<DfaState *, hash<State *>>;

  // Transition table.
  using Table = map<C, DfaState *>;

  constexpr DfaState(int id, bool is_end, uint16_t no)
      : State(id, is_end), no(no){};
  constexpr uint16_t No() const { return no; }
  constexpr Table &Transitions() { return transitions; };
  constexpr DfaState *Next(C c) { return transitions.get(c); }
  constexpr bool HasTransition(C c) { return transitions.has(c); }
  constexpr void AddTransition(C c, DfaState *to) { transitions[c] = to; }

 private:
  Table transitions;
  uint16_t no = 0;  // counter inside a Dfa.
};

// Dfa
class Dfa {
 public:
  DfaState *start = nullptr;
  set<DfaState *, hash<State *>> states;
  set<C> chs;  // Acceptable characters.
  constexpr Dfa(DfaState *start) : start(start){};
  constexpr ~Dfa() noexcept {
    for (auto st : states) delete st;
  }
  // Does this dfa contain given state s?
  constexpr bool Has(DfaState *s) const { return states.has(s); };
  // Returns the number of states.
  constexpr std::size_t Size() const { return states.size(); }
  // Add a new state to the dfa at dfa build time.
  constexpr void AddState(DfaState *st) {
    states.add(st);
    for (auto p : st->Transitions()) chs.add(std::get<0>(p));
  }
  // Match a given string.
  constexpr bool Match(std::string_view s) {
    auto st = start;
    for (auto &c : s) {
      if (!st->HasTransition(c)) return false;
      st = st->Next(c);
    }
    return st->IsEnd();
  }
};

// DfaBuilder builds a dfa from nfa.
class DfaBuilder {
 private:
  // Pointer to the nfa as an input.
  const Nfa *nfa;
  // A hashmap that stores the reachable nfa states from a dfa state through
  // non-epsilon transitions.
  // DfaState id => { C => NfaState pointers }
  map<uint32_t, NfaState::Table> d;
  // Caches all created dfa states.
  // DfaState id => DfaState pointer.
  map<uint32_t, DfaState *> states;
  // EpsilonClosure cache, for performance
  // set(set of NfaState ids) => expanded dfa state id.
  map<uint32_t, uint32_t> epsilon_closure_cache;

  // Generates dfa state id from a given set of nfa states.
  constexpr uint32_t HashNfaStateIds(NfaState::PtrSet &N) {
    // Sorts nfa state's ids.
    std::vector<uint32_t> ids;
    for (auto &st : N) ids.push_back(st->Id());
    std::sort(ids.begin(), ids.end());
    // Makes a hash.
    return hash<std::vector<uint32_t>>{}(ids);
  }

  // Creates a new dfa state from a given set of nfa states.
  constexpr DfaState *NewDfaState(NfaState::PtrSet &N, int id = 0) {
    if (!id) id = HashNfaStateIds(N);

    // The dfa state is an end state if any nfa state is.
    bool is_end = false;
    for (auto &s : N) {
      if (s->IsEnd()) {
        is_end = true;
        break;
      }
    }

    // Find NfaStates can be reachable by non EPSILON transitions.
    for (auto &s : N) {
      for (auto p : s->Transitions()) {
        auto &[c, sts] = p;
        if (c != EPSILON) {
          // Union
          d[id][c].merge(sts);
        }
      }
    }

    // Creates a new DfaState.
    auto st = new DfaState(id, is_end, states.size() + 1);
    states[id] = st;
    return st;
  }

  // Inputs a set of NfaState pointers, expands it inplace through the EPSILON
  // (dfs).
  constexpr void EpsilonClosure(NfaState::PtrSet &N) {
    // Push all nfa states in N to a stack.
    stack<NfaState *> stack;
    for (auto &s : N) stack.push(s);

    // DFS walking.
    while (!stack.empty()) {
      auto s = stack.pop();
      // Find all reachable states via empty symbol EPSILON.
      if (!s->AcceptC(EPSILON)) continue;
      for (auto &t : s->Nexts(EPSILON)) {
        if (!N.has(t)) {
          // If t is not in N, adds it, and pushes it to stack.
          N.add(t);
          stack.push(t);
        }
      }
    }
  };

  // Returns the target DfaState reaches by character c from state S.
  constexpr DfaState *Move(const DfaState *S, C c) {
    // N is the set of nfa states reachable from S through character c.
    auto &N = d.get(S->Id()).get(c);

    // Precheck cache:
    // returns directly if EpsilonClosure for this set has been called.
    auto kid = HashNfaStateIds(N);
    auto p = epsilon_closure_cache.getp(kid);
    if (p != nullptr) return states.get(*p);

    // Makes a copy here, we're going to change it inplace.
    auto N1 = N;

    // Expands via EPSILON.
    EpsilonClosure(N1);

    // Make a DfaState.
    auto id = HashNfaStateIds(N1);
    if (!states.has(id)) {
      // Creates a new DfaState only if it wasn't created before.
      NewDfaState(N1, id);
    }

    // Sets result to cache.
    epsilon_closure_cache.set(kid, id);
    return states.get(id);
  };

 public:
  constexpr DfaBuilder(const Nfa *nfa) : nfa(nfa){};

  // Build a Dfa from Nfa.
  // The created DFA must be released by outer code.
  constexpr Dfa *Build() {
    // Initial state.
    NfaState::PtrSet N0({nfa->start});
    EpsilonClosure(N0);
    auto S0 = NewDfaState(N0);

    // q is the FIFO queue for wait processing dfa states.
    unique_queue<DfaState *, hash<State *>> q;
    q.push(S0);

    // The dfa to build.
    auto dfa = new Dfa(S0);

    while (!q.empty()) {
      auto S = q.pop();
      auto id = S->Id();

      // Non-epsilon transitions.
      auto ptr = d.getp(id);
      if (ptr != nullptr) {
        auto &transitions = *ptr;
        set<C> chs;
        for (auto p : transitions) chs.add(std::get<0>(p));

        // For each non-epsilon transions.
        for (auto c : chs) {
          // Moving to another dfa state T.
          auto T = Move(S, c);
          // Stores the transition.
          S->AddTransition(c, T);
          // Push to processing queue on first meet.
          if (!dfa->Has(T) && !q.has(T)) q.push(T);
        }
      }

      dfa->AddState(S);
    }
    return dfa;
  };
};

// DfaBuilder }}}

// Freezing - FixedDfa and match functions. {{{

// Build a dfa from fixed_string pattern.
// The returned dfa pointer should be finally freed.
template <fixed_string pattern>
constexpr Dfa *build() {
  // Parsing to NFA.
  std::string_view s(pattern.a);
  NfaParser parser;
  auto nfa = parser.Parse(s);
  // Build a DFA.
  DfaBuilder builder(nfa);
  auto dfa = builder.Build();
  return dfa;
}

// Count states and acceptable chars at compiletime to pre-declare the array
// size. So far we have to run build twice in c++ to get the fixed-array size at
// first, and then fill it.
// Returns count of dfa states, count of acceptable chars.
template <fixed_string pattern>
constexpr std::tuple<std::size_t, std::size_t> dfa_count() {
  auto dfa = build<pattern>();
  auto c = std::make_tuple(dfa->Size(), dfa->chs.size());
  delete dfa;
  return c;
}

// 128 is the printable ASCII characters size.
static constexpr std::size_t DefaultAlphabetSize = 128;

// Compiled dfa in format of fixed-size arrays.
// Check function Compile for more documentation.
template <fixed_string pattern, bool pre_index = false,
          std::size_t AlphabetSize = DefaultAlphabetSize>
class FixedDfa {
 private:
  static constexpr auto Ns = dfa_count<pattern>();
  // Number of states.
  static constexpr auto NStates = std::get<0>(Ns);
  // Number of acceptable characters.
  static constexpr auto NChars = std::get<1>(Ns);
  // Size of ch_index_table (atleast to 1 to make clang++ happy).
  static constexpr auto NChIndexTable = pre_index ? AlphabetSize : 1;

  // Character table:
  //    ch_table[short char index - 1] = ch
  // ranges 1~NChars, 0 for invalid.
  std::array<C, NChars> chs;

  // Pre initialized short_char_index table if pre_index is set.
  // ch_index_table[ch] => short_char_index
  std::array<uint8_t, NChIndexTable> ch_index_table;

  // 2D Transition table .
  // transitions[state.no-1][short_char_index-1] => to state
  // states are re-numbered in uint16_t range, start state is numbered to 1.
  std::array<std::array<uint16_t, NChars>, NStates> transitions;

  // Stores whether a state is end state.
  // st_is_end_table[state.no-1] => true/false
  std::array<bool, NStates> st_is_end_table;

 public:
  constexpr FixedDfa() {
    auto dfa = build<pattern>();

    std::array<uint8_t, AlphabetSize> tmp;  // index table

    // init tables with zeros.
    std::fill(tmp.begin(), tmp.end(), 0);
    std::fill(chs.begin(), chs.end(), 0);
    std::fill(ch_index_table.begin(), ch_index_table.end(), 0);
    for (auto &row : transitions) std::fill(row.begin(), row.end(), 0);
    std::fill(st_is_end_table.begin(), st_is_end_table.end(), false);

    // Fill chs.
    for (auto i = 0; auto ch : dfa->chs) chs[i++] = ch;

    // Fill ch_index_table.
    for (auto i = 0; i < chs.size(); i++) tmp[chs[i] % AlphabetSize] = i + 1;
    if (pre_index)  // pre build ch_index_table at static level.
      std::copy_n(tmp.begin(), ch_index_table.size(), ch_index_table.begin());

    // Fill the transition table.
    for (auto st : dfa->states) {
      for (auto p : st->Transitions()) {
        auto &[ch, to] = p;
        auto j = tmp[ch % AlphabetSize];
        transitions[st->No() - 1][j - 1] = to->No();
      }
      if (st->IsEnd()) st_is_end_table[st->No() - 1] = true;
    }

    // Must release the dfa.
    delete dfa;
  }

  // Returns the number of states.
  constexpr std::size_t Size() const { return NStates; }

  // Match a given input string.
  constexpr bool Match(std::string_view s) const {
    // Index table.
    uint8_t *t = nullptr;
    if (pre_index)
      t = const_cast<uint8_t *>(&ch_index_table[0]);
    else {
      t = new uint8_t[AlphabetSize];
      for (auto i = 0; i < AlphabetSize; i++) t[i] = 0;
      for (auto i = 0; i < chs.size(); i++) t[chs[i] % AlphabetSize] = i + 1;
    }

    // Matching.
    auto fail = false;
    uint16_t st = 1;

    for (auto ch : s) {
      auto j = t[ch % AlphabetSize];
      if (j == 0) {
        // ch is not acceptable
        fail = true;
        break;
      }
      uint16_t to = transitions[st - 1][j - 1];
      if (to == 0) {
        // No out transition on this char.
        fail = true;
        break;
      }
      st = to;  // jump ahead
    }
    // release helper table.
    if (!pre_index) delete[] t;
    if (fail) return false;
    return st_is_end_table[st - 1];
  }

  // Match a given input string.
  template <std::size_t N>
  constexpr bool Match(const char (&s)[N]) const {
    std::string_view s1(s);
    return Match(s1);
  }
};

// Freezing }}}
}  // namespace _

// Public API {{{

// Compile time  to build a dfa from a regular expression pattern.
// Example usage:
//
//  // Compile time build.
//  auto dfa = ctre::Compile<"(a|b)*ab">();
//
//  // Runtime matching.
//  dfa.Match("ababab");
//
//  // Compile time matching.
//  constexpr auto b = dfa.Match("ababab");
//
// pre_index indicates whether to build a static character index ahead, this
// makes matching faster but uses more spaces.
// AlphabetSize is the size of alphabet set to use.
template <_::fixed_string pattern, bool pre_index = false,
          std::size_t AlphabetSize = _::DefaultAlphabetSize>
consteval _::FixedDfa<pattern, pre_index, AlphabetSize> Compile() {
  return _::FixedDfa<pattern, pre_index, AlphabetSize>{};
}

// Compile time DFA build and match.
// Example usage:
//  ctre::Match<"(a|b)*ab", "ababab">();
template <_::fixed_string pattern, _::fixed_string s, bool pre_index = false,
          std::size_t AlphabetSize = _::DefaultAlphabetSize>
consteval bool Match() {
  auto dfa = _::build<pattern>();
  std::string_view s1(s.a);
  auto result = dfa->Match(s1);
  delete dfa;
  return result;
}

// Match a given string s by regular expression pattern.
// Example usage:
//
//  // Compile time DFA build and runtime matching.
//  ctre::Match<"(a|b)*ab">("ababab");
//
//  // Compile time DFA build and matching.
//  constexpr auto b = ctre::Match<"(a|b)*ab">("ababab");
//
//  // Compile time DFA build and runtime matching.
//  std::string_view s("ababab");
//  ctre::Match<"(a|b)*ab">(s);
//
//  // Compile time DFA build and matching.
//  constexpr std::string_view s("ababab");
//  constexpr auto b = ctre::Match<"(a|b)*ab">(s);
//
//  // Compile time DFA build and runtime matching.
//  std::string s1;
//  ctre::Match<"(a|b)*ab">(s1);
template <_::fixed_string pattern, bool pre_index = false,
          std::size_t AlphabetSize = _::DefaultAlphabetSize>
constexpr bool Match(std::string_view s) {
  return Compile<pattern>().Match(s);
}

// Public API }}}

}  // namespace ctre
#endif
