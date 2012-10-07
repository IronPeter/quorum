//
//  main.cpp
//  tri
//
//  Created by Петр on 06.10.12.
//  Copyright (c) 2012 Петр. All rights reserved.
//

#include <iostream>
#include <assert.h>
#include <vector>
#include <algorithm>

typedef unsigned char ui8;
typedef unsigned long long docpos;

int cnt = 0;

struct IIterator {
    virtual docpos DocPos() const = 0;
    virtual docpos Skip(docpos doc) = 0;
    virtual ~IIterator() {};
};

struct TPosIterator : public IIterator {
    docpos Position;
    docpos Step;
    TPosIterator() : Position(rand() % 10), Step((rand() % 10) * (rand() % 10) + 1) {
    }
    docpos DocPos() const {
        return Position;
    }
    virtual docpos Skip(docpos doc) {
        ++cnt;
        assert(doc >= Position);
        while (Position < doc) {
            Position += Step;
        }
        return Position;
    }
};

template<size_t Size>
struct THeap {
    docpos Storage[Size];
    size_t Top;
    THeap() : Top(1) {
        for (size_t i = 1; i < Size; ++i) {
            Storage[i] = docpos(-1);
        }
        Storage[0] = 0;

    }
    bool Clean() const {
        if (Storage[0] != 0) {
            return false;
        }
        for (size_t i = 1; i < Size; ++i) {
            if (Storage[i] != docpos(-1)) {
                return false;
            }
        }
        return true;
    }
    void Clear() {
        for (size_t i = 1; i <= Top; ++i) {
            Storage[i] = docpos(-1);
        }
        Storage[0] = 0;
        Top = 1;
    }
    void Traverse(size_t index) {
        printf("%lld %lld %lld\n", Storage[index], Storage[index * 2 + 0], Storage[index * 2 + 1]);
        if (index < Top) {
            Traverse(index * 2 + 0);
            Traverse(index * 2 + 1);
        }
    }
    void Out() {
        Traverse(1);
    }
    docpos Current() const {
        return Storage[1];
    }
    void Drain() {
        docpos newres = Storage[--Top];
        size_t index = 1;
        docpos hi = newres;
        while (1) {
            size_t left = index * 2 + 0;
            size_t rght = index * 2 + 1;
            docpos sleft = Storage[left];
            docpos srght = Storage[rght];
            size_t newindex = sleft < srght ? left : rght;
            docpos lo = sleft < srght ? sleft : srght;
            if (hi > lo) {
                Storage[index] = lo;
                index = newindex;
            } else {
                Storage[index] = hi;
                Storage[Top] = docpos(-1);
                break;
            }
        }
    }
    void Add(docpos value) {
        size_t index = Top++;
        docpos hi = value;
        while (1) {
            size_t newindex = index / 2;
            docpos lo = Storage[newindex];
            if (hi < lo) {
                Storage[index] = lo;
                index = newindex;
            } else {
                Storage[index] = hi;
                break;
            }
        }
    }
};

template<size_t Size>
struct TIteratorHeap {
    IIterator *Iterators[Size];
    size_t Indices[Size];
    size_t Count;
    int ObjectCount;
    int Accum[Size];
    int Weights[Size];
    int Quorum;
    docpos Positions[Size];
    THeap<Size * 4> Heap;
    bool Free[Size];
    
    int ImplicitQuorum(docpos doc) {
        int accum[Size];
        for (size_t i = 0; i < ObjectCount; ++i) {
            accum[i] = 0;
        }
        for (size_t i = 0; i < Count; ++i) {
            docpos pos = Iterators[i]->DocPos();
            accum[Indices[i]] += (pos == doc) ? Weights[i] : 0;
        }
        int quorum = 0;
        for (size_t i = 0; i < ObjectCount; ++i) {
            quorum += Remap(accum[i]);
        }
        return quorum;
    }


    int Remap(int val) {
        return val + ((val != 0) ? 10 : 0);
    }

    int Modify(int deltaWeight, size_t index) {
        size_t remap = Indices[index];
        int oldValue = Accum[remap];
        int newValue = oldValue + deltaWeight;
        Accum[remap] = newValue;
        return Remap(newValue) - Remap(oldValue);
    }

    int Transfer(docpos currentPos) {
        Heap.Drain();
        size_t index = currentPos & 0xffff;
        Free[index] = true;
        return Modify(Weights[index], index);
    }

    bool QuorumStep(docpos loBound, docpos hiBound, docpos &foundDoc, int &accum) {

        docpos currentDoc = 0;
        bool goodEnd = true;
        
        while (1) {
            docpos elementPos = Heap.Current();
            docpos elementDoc = elementPos >> 16;
            if (elementDoc != currentDoc) {
                if (accum >= Quorum && elementDoc >= loBound) {
                    if (currentDoc < loBound) {
                        currentDoc = loBound;
                    }
                    break;
                }
                if (elementDoc > hiBound) {
                    currentDoc = hiBound;
                    goodEnd = false;
                    break;
                }
                currentDoc = elementDoc;
            }
            accum += Transfer(elementPos);
        }
        

        for (size_t index = 0; index < Count; ++index) {
            if (Free[index]) {
                docpos elementDoc = Iterators[index]->Skip(currentDoc);
                if (elementDoc > currentDoc) {
                    int delta = Modify(-Weights[index], index);
                    accum += delta;
                    Heap.Add((elementDoc << 16) + index);
                    Free[index] = false;
                    if (accum < Quorum && goodEnd) {
                        foundDoc = currentDoc;
                        return false;
                    }
                }
            }
        }

        foundDoc = currentDoc;
        return true;
    }

    bool FindDoc(docpos loBound, docpos hiBound, docpos &position) {
        Heap.Clear();
        assert(Heap.Clean());
        
        for (size_t i = 0; i < Count; ++i) {
            docpos pos = Iterators[i]->DocPos();
            Heap.Add((pos << 16) + i);
            Accum[i] = 0;
            Free[i] = false;
        }
        
        int accum = 0;
        assert(0 == Remap(0));
        while (1) {
            docpos foundDoc = loBound;
            bool result = QuorumStep(loBound, hiBound, foundDoc, accum);
            if (result) {
                position = foundDoc;
                return accum >= Quorum;
            }
        }
    }
};

struct TIterInfo {
    IIterator *Iter;
    size_t Index;
    int Weight;
    float Prob;
    bool operator < (const TIterInfo &info) const {
        return Prob < info.Prob;
    }
};

int main(int argc, const char * argv[]) {
    int mode = 0;
    if (argc > 1) {
        mode = atoi(argv[1]);
    }
    for (size_t j = 0; j < 10000; ++j) {
        TIteratorHeap<256> heap;
        TPosIterator iters[16];
        std::vector<TIterInfo> infos;
        for (size_t i = 0; i < 16; ++i) {
            TIterInfo info;
            if (mode == 0) {
                info.Prob =  1.0f / iters[i].Step;
            } else if (mode == 1) {
                info.Prob = -iters[i].Step;
            } else {
                info.Prob = iters[i].Step;
            }
            info.Index = i / 2;
            info.Iter = &iters[i];
            info.Weight = 4;
            infos.push_back(info);
        }
        std::sort(infos.begin(), infos.end());
        for (size_t i = 0; i < 16; ++i) {
            heap.Indices[i] = infos[i].Index;
            heap.Iterators[i] = infos[i].Iter;
            heap.Weights[i] = infos[i].Weight;
        }
        heap.Count = 16;
        heap.Quorum = 80;
        heap.ObjectCount = 8;

        docpos pos = docpos(-1);
        for (size_t k = 0; k < 100; ++k) {
            bool result = heap.FindDoc(pos + 1, 100000, pos);
            if (!result) {
                break;
            }
        }
        std::cout << pos << "\n";
    }
    std::cout << cnt << "\n";
    return 0;
}

