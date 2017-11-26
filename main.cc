
#include <simlib.h>

#define T_PM 10
#define T_BUFFER 12
#define T_L1_B 10
#define T_METAL_SHEET_GENERATOR 300
#define T_STACKER_CRANE 30


//pocet plechu na palete
#define C_SHEET_METAL_PALET 10

//kapacity skladu pro jednotlive druhy palet
#define C_SHEET_METAL_PALLET_STORE 10
#define C_SKELETON_PALLET_STORE 10
#define C_RESULT_PALLET_STORE 10
#define C_EMPTY_PALLET_STORE 10

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

//pomocne pocitatlo, ktere
Store processedSheetMetalBufferCounter("processedSheetMetalBufferCounter", C_SHEET_METAL_PALET);

//flag, ktery rika, jsou-li k dispozici prazdne palety pro nalozeni z PM
Store hasPAndSPalet("hasPAndSPalet", 2);

//flagy, je-li mozne privest dalsi paletu pro resulty a skeletony do Separatoru
Store canLoadNextSkeletonPallet("canLoadNextSkeletonPallet", 1); //chceme defaultne 1!
Store canLoadNextResultPallet("canLoadNextResultPallet", 1); //chceme defaultne 1!

//zasobni resultu a skeletonu v Separatoru - vystup z punchign machine
Store skeletonStack("skeletonStack", C_SHEET_METAL_PALET);
Store resultStack("resultStack", C_SHEET_METAL_PALET);

//sklady palet - v initu simulace se vyprazdni(sebere se jim vsechna kapacita)
//nasledne se paletu do skladu plni
//Leave uklada paletu do skladu, Enter ji vytahuje
Store metalSheetPalletStore("metalSheetPalletStore", C_SHEET_METAL_PALLET_STORE);
Store emptyPalletStore("emptyPalletStore", C_EMPTY_PALLET_STORE);
Store skeletonPalletStore("skeletonPalletStore", C_SKELETON_PALLET_STORE);
Store resultPalletStore("resultPalletStore", C_RESULT_PALLET_STORE);

//stacker crane - obsluha skladu a linek
Queue stackerCraneQueue("stackerCraneQueue");
Facility stackerCrane("stackerCrane", stackerCraneQueue);

//flag pro stacker crane, ma-li poslat plnou paletu s plechy do bufferu
Store doSendNextMetalSheetPallet("doSendNextMetalSheetPallet", 1); //chceme defaultne 1

class EmptyPallet : public Process {
    void Behavior() {
        //jede z bufferu
        this->Wait(T_L1_B);
        //ceka na SC
        this->Seize(stackerCrane);
        //jede v SC a umistuje se do skladu
        this->Wait(T_STACKER_CRANE);
        //prazdna paleta se vlozi do skladu (Leave symbolizuje vraceni kapacity skladu)
        this->Leave(emptyPalletStore);
        //uvolnime SC
        this->Release(stackerCrane);
        //nastavime flag, ze muzeme poslat dalsi paletu s plechy
        doSendNextMetalSheetPallet.Leave(1);

        //RIP EMPTY PALET
    }
};


class SkeletonPallet : public Process {

private:
    void Behavior() {
        //pokud je stack plny, vyprazdnime ho a muzeme odvest paletu
        if (skeletonStack.Empty()) {
            skeletonStack.Enter(this, C_SHEET_METAL_PALET);
//            canLoadNextSkeletonPallet.Leave(1); //todo: odkomentovat az budou vstupy do separatoru
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

        //vlozime plech do counteru bufferu - ve schematu nazvano Joiner
        processedSheetMetalBufferCounter.Leave(1);

        //todo: seize POST PM workera?

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
        //vlozime paletu do skladu
        this->Leave(metalSheetPalletStore);
        //cekame, nez bude mozne poslat paletu na linky
        this->Enter(doSendNextMetalSheetPallet);
        //zabereme SC
        this->Seize(stackerCrane);
        //mame SC, jedeme
        this->Wait(T_STACKER_CRANE);
        //vratime SC
        this->Release(stackerCrane);
        //jedeme na lince do bufferu
        this->Wait(T_L1_B);

        //zabereme buffer pred SC
        this->Seize(pmBuffer);

        //vyskladame plechy z palety
        for (int i = 0; i < C_SHEET_METAL_PALET; ++i) {
            (new MetalSheet())->Activate();
        }

        //pockame, nez se zpracuji vsechny plechy
        processedSheetMetalBufferCounter.Enter(this, C_SHEET_METAL_PALET);

        //vytvorime prazdnou paletu
        (new EmptyPallet())->Activate();
        //opustime buffer
        this->Release(pmBuffer);

        return; //RIP paleta
    }
};

//todo: uz nebude nutny! aktualne naplanuje generovani palet s plechama
class MetalSheetPalletGenerator : public Event {
public:
    double Interval;

    MetalSheetPalletGenerator(double interv) : Event() {
        Interval = interv;
    };

    void Behavior() {
        for (int i = 0; i < C_SHEET_METAL_PALLET_STORE; ++i) {
            (new MetalSheetPallet())->Activate(Time + Exponential(Interval) * (i));
        }
    }
};

class SimulationInit : public Event {
public:

    void Behavior() {
        //vybereme sklady, protoze je potrebujeme prazdne
        processedSheetMetalBufferCounter.Enter(this, C_SHEET_METAL_PALET);
        skeletonStack.Enter(this, C_SHEET_METAL_PALET);
        resultStack.Enter(this, C_SHEET_METAL_PALET);
        canLoadNextSkeletonPallet.Enter(this, 1);
        canLoadNextResultPallet.Enter(this, 1);

        //vyprazdnime sklady, protoze je naplnime jinak
        metalSheetPalletStore.Enter(this, C_SHEET_METAL_PALLET_STORE);
        emptyPalletStore.Enter(this, C_EMPTY_PALLET_STORE);
        skeletonPalletStore.Enter(this, C_SKELETON_PALLET_STORE);
        resultPalletStore.Enter(this, C_RESULT_PALLET_STORE);

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

    separator.Output();
    separatorQueue.Output();

    hasPAndSPalet.Output();
    canLoadNextSkeletonPallet.Output();
    canLoadNextResultPallet.Output();
    skeletonStack.Output();
    resultStack.Output();

    stackerCrane.Output();
    stackerCraneQueue.Output();

    metalSheetPalletStore.Output();
    emptyPalletStore.Output();
    skeletonPalletStore.Output();
    resultPalletStore.Output();

    doSendNextMetalSheetPallet.Output();
}
