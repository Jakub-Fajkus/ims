
#include <simlib.h>

#define T_PM 10
#define T_BUFFER 12
#define T_METAL_SHEET_GENERATOR 300
#define C_SHEET_METAL_PALET 10

Queue pmQueue("pmQueue");
Facility pm("Punching machine", pmQueue);

Queue pmBufferQueue("pmBufferQueue");
Facility pmBuffer("pmBuffer", pmBufferQueue);

Queue pmLoadingBufferQueue("pmLoadingBufferQueue");
Facility pmLoadingBuffer("pmLoadingBuffer", pmLoadingBufferQueue);

Queue pmPreQueue("pmPreQueue");
Facility pmPreWorker("pmPreWorker", pmPreQueue);

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
        this->Seize(pmPreWorker);
        //cas nakladani do PM
        this->Wait(T_BUFFER);
        this->Release(pmLoadingBuffer);

        //zabereme PM
        this->Seize(pm);

        //vlozime plech do counteru
        processedSheetMetalCounter.Leave(1);

        //uvonime pre workera
        this->Release(pmPreWorker);
        this->Wait(T_PM);
        this->Release(pm);
    }
};


class SheetMetalPallet : public Process {

private:
    void Behavior() {
        this->Seize(pmBuffer);

        for (int i = 0; i < C_SHEET_METAL_PALET; ++i) {
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
    bool did = false;

    SheetMetalPalletGenerator(double interv) : Event() {
        Interval = interv;
    };

    void Behavior() {
        //vybereme sklad, protoze ho potrebujeme prazdny

        (new SheetMetalPallet())->Activate();
        Activate(Time + Exponential(Interval));
    }
};

class SimulationInit : public Event {
public:
    bool did = false;

    void Behavior() {
        //vybereme sklad, protoze ho potrebujeme prazdny
        if (!did) {
            processedSheetMetalCounter.Enter(this, C_SHEET_METAL_PALET);
            did=true;
        }

        (new SheetMetalPalletGenerator(T_METAL_SHEET_GENERATOR))->Activate();
    }
};



int main() {
    SetOutput("main.dat");
    int a;

    Init(0, 10000);

    (new SimulationInit())->Activate();

    Run();

    pmBuffer.Output();
    pmBufferQueue.Output();

    pmLoadingBuffer.Output();
    pmLoadingBufferQueue.Output();

    pmPreWorker.Output();
    pmPreQueue.Output();

    pm.Output();
    pmQueue.Output();

    processedSheetMetalCounter.Output();
    emptyPalletQueue.Output();
}
