
#include <simlib.h>

#define T_PM 10
#define T_BUFFER 12
#define C_SHEET_METAL_PALET 45

Queue pmQueue("pmQueue");
Facility pm("Punching machine", pmQueue);

Queue pmBufferQueue("pmBufferQueue");
Facility pmBuffer("pmBuffer", pmBufferQueue);

Queue pmLoadingBufferQueue("pmLoadingBufferQueue");
Facility pmLoadingBuffer("pmLoadingBuffer", pmLoadingBufferQueue);

Store processedSheetMetalCounter("Sheet metal counter", C_SHEET_METAL_PALET);


Queue emptyPalletQueue("emptyPalletQueue");

class EmptyPallet : public Process {
    void Behavior() {
        //todo: paleta nekam jde
        emptyPalletQueue.Insert(this);
    }
};

class MetalSheet : public Process {
    void Behavior() {
        //zabereme loading buffer
        this->Seize(pmLoadingBuffer);
        this->Release(pmLoadingBuffer);

        //vlozime plech do counteru
        processedSheetMetalCounter.Leave(1);

        //zabereme PM
        this->Seize(pm);
        this->Wait(Exponential(T_PM));
        this->Release(pm);
    }
};


class SheetMetalPallet : public Process {
    int count;

public:
    SheetMetalPallet(Priority_t p, int count) : Process(p), count(count) {}

private:
    void Behavior() {
        this->Seize(pmBuffer);

        for (int i = 0; i < this->count; ++i) {
            (new MetalSheet())->Activate();
        }

        //pockame, nez se zpracuji vsechny plechy
        processedSheetMetalCounter.Enter(this, C_SHEET_METAL_PALET);

        (new EmptyPallet())->Activate();
        this->Release(pmBuffer);
    }
};


class SheetMetalPalletGenerator : public Event {
public:
    double Interval;

    SheetMetalPalletGenerator(double interv) : Event() {
        Interval = interv;
    };

    void Behavior() {
        (new SheetMetalPallet(DEFAULT_PRIORITY, C_SHEET_METAL_PALET))->Activate();
        Activate(Time + Exponential(Interval));
    }
};


int main() {
    SetOutput("main.dat");
    int a;

    //for (a=0; a<Sanitek; a++)
    //	Sanitka[a].SetQueue(&cekani);

    Init(0, 1000);

    (new SheetMetalPalletGenerator(12))->Activate();

    Run();

//    cekani.Output();
    pm.Output();
    pmQueue.Output();

    pmBuffer.Output();
    pmBufferQueue.Output();

    pmLoadingBuffer.Output();
    pmLoadingBufferQueue.Output();

    processedSheetMetalCounter.Output();
    emptyPalletQueue.Output();
}
