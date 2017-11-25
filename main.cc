
#include <simlib.h>

#define T_PM 10
#define T_BUFFER 12
#define T_METAL_SHEET_GENERATOR 300

#define C_SHEET_METAL_PALET 2

Queue pmQueue("pmQueue");
Facility pm("Punching machine", pmQueue);

Queue pmBufferQueue("pmBufferQueue");
Facility pmBuffer("pmBuffer", pmBufferQueue);

Queue pmLoadingBufferQueue("pmLoadingBufferQueue");
Facility pmLoadingBuffer("pmLoadingBuffer", pmLoadingBufferQueue);

Queue pmPreQueue("pmPreQueue");
Facility pmPreWorker("pmPreWorker", pmPreQueue);

Queue separatorQueue("separatorQueue");
Facility separator("separator", separatorQueue);

Store processedSheetMetalBufferCounter("processedSheetMetalBufferCounter", C_SHEET_METAL_PALET);

Store hasPAndSPalet("hasPAndSPalet", 2);
Store canLoadNextSkeletonPallet("canLoadNextSkeletonPallet", 1); //chceme defaultne 1!
Store canLoadNextResultPallet("canLoadNextResultPallet", 1); //chceme defaultne 1!

Store skeletonStack("skeletonStack", C_SHEET_METAL_PALET);
Store resultStack("resultStack", C_SHEET_METAL_PALET);

Queue emptyPalletQueue("emptyPalletQueue"); //todo: temp

class EmptyPallet : public Process {
    void Behavior() {
        //todo: paleta nekam jde
        emptyPalletQueue.Insert(this);
    }
};


class SkeletonPallet : public Process {

private:
    void Behavior() {
        //pokud je stack plny, vyprazdnime ho a muzeme odvest paletu
        if (skeletonStack.Empty()) {
            skeletonStack.Enter(this, C_SHEET_METAL_PALET);
//            canLoadNextSkeletonPallet.Leave(1); //todo: odkomentovat az budou vstupy
            //todo: paleta nekam musi jit

            //temp : nasimulujeme vytvoreni nove palety!!
            (new SkeletonPallet())->Activate();
        } else {
            hasPAndSPalet.Leave(1);
            return; //RIP palet
        }
    }
};


class ResultPallet : public Process {

private:
    void Behavior() {
        //pokud je stack plny, vyprazdnime ho a muzeme odvest paletu
        if (resultStack.Empty()) {
            resultStack.Enter(this, C_SHEET_METAL_PALET);
//            canLoadNextResultPallet.Leave(1); //todo: odkomentovat az budou vstupy
            //todo: paleta nekam musi jit

            //temp: nasimulujeme vytvoreni nove palety!!
            (new ResultPallet())->Activate();
        } else {
            hasPAndSPalet.Leave(1);
            return; //RIP palet
        }
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

        //vlozime plech do counteru bufferu
        processedSheetMetalBufferCounter.Leave(1);

        //uvonime pre workera
        this->Release(pmPreWorker);
        this->Wait(T_PM);
        this->Release(pm);

        //zabereme separator
        this->Seize(separator);
        //pockame, az budeme mit k dispozici prazdne paletu pro resulty a skeletony
        hasPAndSPalet.Enter(this, 2);

        //vlozime resulty a skeletony do bufferu
        skeletonStack.Leave(1);
        resultStack.Leave(1);

        //vytvorime procesy pro paletu se skeletony a resulty
        (new SkeletonPallet())->Activate();
        (new ResultPallet())->Activate();

        this->Release(separator);

        return;//RIP plech
    }
};


class MetalSheetPallet : public Process {

private:
    void Behavior() {
        this->Seize(pmBuffer);

        for (int i = 0; i < C_SHEET_METAL_PALET; ++i) {
            (new MetalSheet())->Activate();
        }

        //pockame, nez se zpracuji vsechny plechy
        processedSheetMetalBufferCounter.Enter(this, C_SHEET_METAL_PALET);

        (new EmptyPallet())->Activate();
        this->Release(pmBuffer);

        return; //RIP paleta
    }
};


class MetalSheetPalletGenerator : public Event {
public:
    double Interval;
    bool did = false;

    MetalSheetPalletGenerator(double interv) : Event() {
        Interval = interv;
    };

    void Behavior() {
        //vybereme sklad, protoze ho potrebujeme prazdny

        (new MetalSheetPallet())->Activate();
        Activate(Time + Exponential(Interval));
    }
};

class SimulationInit : public Event {
public:
    bool did = false;

    void Behavior() {
        if (!did) {
            //vybereme sklady, protoze je potrebujeme prazdne
            processedSheetMetalBufferCounter.Enter(this, C_SHEET_METAL_PALET);
//            hasPAndSPalet.Enter(this, 2);
            skeletonStack.Enter(this, C_SHEET_METAL_PALET);
            resultStack.Enter(this, C_SHEET_METAL_PALET);
            canLoadNextSkeletonPallet.Enter(this, 1);
            canLoadNextResultPallet.Enter(this, 1);

            //todo: tmp!
//            (new SkeletonPallet())->Activate();
//            (new ResultPallet())->Activate();


            did = true;
        }

        (new MetalSheetPalletGenerator(T_METAL_SHEET_GENERATOR))->Activate();
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

    processedSheetMetalBufferCounter.Output();
    emptyPalletQueue.Output();

    separator.Output();
    separatorQueue.Output();

    hasPAndSPalet.Output();
    canLoadNextSkeletonPallet.Output();
    canLoadNextResultPallet.Output();
    skeletonStack.Output();
    resultStack.Output();
}
