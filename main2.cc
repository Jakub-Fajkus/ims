
#include <simlib.h>
#include <ctime>

#define MINUTE (60)
#define HOUR (MINUTE * 60)
#define DAY (HOUR * 24)

#define T_PM 1.5 * MINUTE
#define T_BUFFER 5

//casy prevozu na lince
#define T_L1_B 20
#define T_L2_S 15

#define T_STACKER_CRANE 30
#define T_SEPARATOR 2

#define C_ORDERS 2000
#define C_SHEET_METAL_PALET 10
//kapacity skladu pro jednotlive druhy palet
#define C_SHEET_METAL_PALLET_STORE 200
#define C_SKELETON_PALLET_STORE 200
#define C_RESULT_PALLET_STORE 200
#define C_EMPTY_PALLET_STORE  C_SHEET_METAL_PALLET_STORE + C_SKELETON_PALLET_STORE + C_RESULT_PALLET_STORE//soucet vsech palet v systemu

Histogram delkaZpracovaniPlechu("Delka zpracovani plechu", 0, 40, 2000);
Histogram pocetPaletVZakazce("Pocet palet v zakazce", 0, 2, 50);

Queue beforePmWorkerQueue("beforePmWorkerQueue");
Facility beforePmWorker("beforePmWorker", beforePmWorkerQueue);

Queue afterPmWorkerQueue("afterPmWorkerQueue");
Facility afterPmWorker("afterPmWorker", afterPmWorkerQueue);

Queue pm1Queue("pm1Queue");
Facility pm1("Punching machine 1", pm1Queue);

Queue pmBufferQueue("pmBufferQueue");
Facility pmBuffer("pmBuffer", pmBufferQueue);

Queue pmLoadingBufferQueue("pmLoadingBufferQueue");
Facility pmLoadingBuffer("pmLoadingBuffer", pmLoadingBufferQueue);


Queue pm2Queue("pm2Queue");
Facility pm2("Punching machine 2", pm2Queue);


Queue separatorQueue("separatorQueue");
Facility separator("separator", separatorQueue);

//pomocne pocitatlo, ktere
Store processedSheetMetalBufferCounter("processedSheetMetalBufferCounter", C_SHEET_METAL_PALET);

//flag, ktery rika, jsou-li k dispozici prazdne palety pro nalozeni z PM
Store hasPAndSPalet("hasPAndSPalet", 2);

//flagy, je-li mozne privest dalsi paletu pro resulty a skeletony do Separatoru
//leave nastavuje flag
//enter jej shazuje
Store canLoadNextSkeletonPallet("canLoadNextSkeletonPallet", 1); //chceme defaultne 1!
Store canLoadNextResultPallet("canLoadNextResultPallet", 1); //chceme defaultne 1!

//zasobni resultu a skeletonu v Separatoru - vystup z punchign machine
Store skeletonStack("skeletonStack", C_SHEET_METAL_PALET);
Store resultStack("resultStack", C_SHEET_METAL_PALET);

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

Store orders("orders", C_ORDERS);

class EmptyMetalSheetPallet : public Process {
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

public:
    EmptyMetalSheetPallet(int i) {

    }
};


class SkeletonPallet : public Process {
public:
    SkeletonPallet(Priority_t p) : Process(p) {}

private:
    void Behavior() {
        //pokud je stack plny plechu, vyprazdnime ho a muzeme odvest paletu
        //plechy ze skladu odebiraji kapacitu, proto mame pdominku na Empty
        if (skeletonStack.Empty()) {
            //vybereme vsechny plechy
            skeletonStack.Enter(this, C_SHEET_METAL_PALET);
            //jedeme na lince k SC
            this->Wait(T_L2_S);
            //pockame na SC
            this->Seize(stackerCrane);
            //jedeme v SC
            this->Wait(T_STACKER_CRANE);
            //vratime SC
            this->Release(stackerCrane);
            //nastavime flag, ze je linka volna
            this->Leave(canLoadNextSkeletonPallet);
            //vlozime paletu do skladu
            this->Leave(skeletonPalletStore);

            return; //RIP palet
        } else {
            hasPAndSPalet.Leave(1);
            return; //RIP palet
        }
    }
};

class EmptySkeletonPallet : public Process {
public:
    EmptySkeletonPallet(Priority_t p) : Process(p) {}

private:
    void Behavior() {
        //vytahneme paletu ze skladu
        this->Enter(emptyPalletStore);
        //zabereme SC
        this->Seize(stackerCrane);
        //mame SC, jedeme
        this->Wait(T_STACKER_CRANE);
        //vratime SC
        this->Release(stackerCrane);
        //jedeme na lince do bufferu
        this->Wait(T_L2_S);
        //zmenime prazdnou paletu na paletu se S
        (new SkeletonPallet(7))->Activate();

        return; //RIP palet
    }
};

//sleduje, je-li potreba vytahnout prazdnou paletu ze skladu a udelat z ni prazdnou paletu pro skeletony
class EmptySkeletonPalletWatcher : public Process {
private:
    void Behavior() {
        //cekame, nez bude potreba pradna paleta pro skeletony
        this->Enter(canLoadNextSkeletonPallet);
        //vytvorime prazdnou paletu pro skeletony
        (new EmptySkeletonPallet(5))->Activate();

        //vytvori se novy watcher
        (new EmptySkeletonPalletWatcher())->Activate();

        return; //RIP watcher
    }
};


class ResultPallet : public Process {
public:
    ResultPallet(Priority_t p) : Process(p) {}

private:
    void Behavior() {
        if (resultStack.Empty()) {
            //vybereme vsechny plechy
            resultStack.Enter(this, C_SHEET_METAL_PALET);
            //jedeme na lince k SC
            this->Wait(T_L2_S);
            //pockame na SC
            this->Seize(stackerCrane);
            //jedeme v SC
            this->Wait(T_STACKER_CRANE);
            //vratime SC
            this->Release(stackerCrane);
            //nastavime flag, ze je linka volna
            this->Leave(canLoadNextResultPallet);
            //vlozime paletu do skladu
            this->Leave(resultPalletStore);

            return; //RIP palet
        } else {
            hasPAndSPalet.Leave(1);
            return; //RIP palet
        }
    }
};

class EmptyResultPallet : public Process {
public:
    EmptyResultPallet(Priority_t p) : Process(p) {}

private:
    void Behavior() {
        //vytahneme paletu ze skladu
        this->Enter(emptyPalletStore);
        //zabereme SC
        this->Seize(stackerCrane);
        //mame SC, jedeme
        this->Wait(T_STACKER_CRANE);
        //vratime SC
        this->Release(stackerCrane);
        //jedeme na lince do bufferu
        this->Wait(T_L2_S);
        //zmenime prazdnou paletu na paletu se S
        (new ResultPallet(8))->Activate();

        return; //RIP palet
    }
};

//sleduje, je-li potreba vytahnout prazdnou paletu ze skladu a udelat z ni prazdnou paletu pro resulty
class EmptyResultPalletWatcher : public Process {
private:
    void Behavior() {
        //cekame, nez bude potreba pradna paleta pro resulty
        this->Enter(canLoadNextResultPallet);
        //vytvorime prazdnou paletu pro resulty
        (new EmptyResultPallet(6))->Activate();

        //vytvori se novy watcher
        (new EmptyResultPalletWatcher())->Activate();

        return; //RIP watcher
    }
};

class MetalSheetPallet : public Process {
public:
    int difficulty;

    MetalSheetPallet(Priority_t p, int difficulty) : Process(p), difficulty(difficulty) {}

private:
    void Behavior();
};


class MetalSheet : public Process {
public:
    int difficulty;
    MetalSheetPallet *containingPallet;
    double processingTime;

    MetalSheet(Priority_t p, int difficulty, MetalSheetPallet *containingPallet) : Process(p), difficulty(difficulty),
                                                                                   containingPallet(containingPallet) {}

    void Behavior() {
        double startTime = Time;

        //zabereme loading buffer
        this->Seize(pmLoadingBuffer);
        //cas nakladani do PM
        this->Wait(T_BUFFER);
        this->Release(pmLoadingBuffer);

        //zabereme workera, ktery rozdeluje plechy mezi 2 PM
        this->Seize(beforePmWorker);

        //todo: wait!
        this->Wait(10);

        //vybereme, kterou PM zabereme
        Facility *usedPm;
        if (!pm1.Busy()) {
            usedPm = pm1;
        } else {
            usedPm = pm2;
        }

        //zabereme vybranou PM
        this->Seize(*usedPm);

        //vratime workera na prerozdelovani
        this->Release(beforePmWorker);

        //vlozime plech do counteru bufferu - ve schematu nazvano Joiner
        processedSheetMetalBufferCounter.Leave(1);

        //poud je to poslendi plech, aktivujeme paletu, na kter prijel
        if (processedSheetMetalBufferCounter.Empty()) {
            this->containingPallet->Activate();
        }

        //palime plechy v PM
        this->Wait(T_PM * this->difficulty);

        //vratime PM
        this->Release(*usedPm);

        //zabereme workera, ktery sklada vysledky z obou PM
        this->Seize(afterPmWorker);

        //todo: wait
        this->Wait(10);



        //vratime workera
        this->Release(afterPmWorker);


        //###################### CAST V SEPARATORU #####################################################################

        //pockame, az budeme mit k dispozici prazdne paletu pro resulty a skeletony
        hasPAndSPalet.Enter(this, 2);

        //zabereme separator
        this->Seize(separator);

        //cekame na praci separatoru
        this->Wait(T_SEPARATOR + (T_SEPARATOR * 2 * this->difficulty));

        //vlozime resulty a skeletony do bufferu
        skeletonStack.Leave(1);
        resultStack.Leave(1);

        //vytvorime procesy pro paletu se skeletony a resulty
        (new SkeletonPallet(7))->Activate();
        (new ResultPallet(8))->Activate();

        this->Release(separator);

        this->processingTime = Time - startTime;
        delkaZpracovaniPlechu(this->processingTime);

        return;//RIP plech
    }
};


void MetalSheetPallet::Behavior() {
    //vytahneme paletu ze skladu
    this->Enter(metalSheetPalletStore);
    //cekame, nez bude mozne poslat paletu na linky
    this->Enter(doSendNextMetalSheetPallet);
    //zabereme SC
    this->Seize(stackerCrane);
    //mame SC, jedeme
    this->Wait(T_STACKER_CRANE);
    //vratime SC
    this->Release(stackerCrane);
    //jedeme na lince do bufferu - uz bez SC
    this->Wait(T_L1_B);
    //zabereme buffer pred SC
    this->Seize(pmBuffer);
    //vyskladame plechy z palety
    for (int i = 0; i < C_SHEET_METAL_PALET; ++i) {
        (new MetalSheet(1, this->difficulty, this))->Activate();
    }

    //pasivujeme se a pockame, nez nas plech aktivuje
    this->Passivate();
    //zde mne plech aktivuje

    //pockame, nez se zpracuji vsechny plechy
    processedSheetMetalBufferCounter.Enter(this, C_SHEET_METAL_PALET);

    //vytvorime prazdnou paletu
    (new EmptyMetalSheetPallet(10))->Activate();
    //opustime buffer
    this->Release(pmBuffer);

    return; //RIP paleta
}


class OrderGenerator : public Event {
public:
    void Behavior() {
        //vuytvorime objednavku
        orders.Leave(1);
        //naplanujeme dalsi vygenerovnai objednavky
        this->Activate(Time + Exponential(8*HOUR));
    }
};

class HumanWorker : public Process {
private:
    //todo: odstranit pragmy!
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"

    void Behavior() {
        unsigned long palletCount;
        bool isProcessingOrder = false;

        while (true) {
            //je-li nejaka objednavka
            if (!orders.Full() && !isProcessingOrder) {
                isProcessingOrder = true;
                //nahodne vygenerujeme jeji parametry
                //nejmensi objednavka je jedna paleta, nejvetsi je limitovana veliksoti skladu
                palletCount = (unsigned long) Uniform(1, C_RESULT_PALLET_STORE);
//                palletCount = 10;
                int orderDifficulty = (int) Uniform(1, 6);
//                int orderDifficulty = 1;

                //vezmeme ze skladu prazdne palety
                this->Enter(emptyPalletStore, palletCount);

                //zabereme SC
                this->Seize(stackerCrane);

                //toto nakladani nejakou chvili trva
                this->Wait(palletCount * (T_STACKER_CRANE + 10));

                //vratime SC
                this->Release(stackerCrane);

                //vlozime do skladu palety s plechy
                this->Leave(metalSheetPalletStore, palletCount);

                pocetPaletVZakazce(palletCount);

                //vytvorime do systemu palety s plechy
                for (int i = 0; i < palletCount; ++i) {
                    (new MetalSheetPallet(9, orderDifficulty))->Activate();
                }
            } //je-li nejaka objednavka

            //pracovnik si jde neco vyridit
            //nebo od stroje odvazi vysledky a skeletony
            //nebo k nemu dovazi plechy pro dalsi zakazku
            this->Wait(Exponential(1 * MINUTE));

            //jsou-li vsechny palety s vysledky jiz ve skladu
            if (resultPalletStore.Used() == C_RESULT_PALLET_STORE - palletCount) {
                //vybereme resulty ze skladu
                this->Enter(resultPalletStore, palletCount);
                //zabereme SC
                this->Seize(stackerCrane);
                //vykladame resulty
                this->Wait(palletCount * (T_STACKER_CRANE + 10));
                //vratime SC
                this->Release(stackerCrane);
                //vlozime prazdne palety
                this->Leave(emptyPalletStore, palletCount);

                //vybereme skeletony ze skladu
                this->Enter(skeletonPalletStore, palletCount);
                //zabereme SC
                this->Seize(stackerCrane);
                //vykladame skeletony
                this->Wait(palletCount * (T_STACKER_CRANE + 10));
                //vratime SC
                this->Release(stackerCrane);
                //vlozime prazdne palety
                this->Leave(emptyPalletStore, palletCount);

                isProcessingOrder = false;
            }
        }
        //lide u nas neumiraji!
    }

#pragma clang diagnostic pop
};


class Fault : public Process {
public:
    Facility &facility;
    int failureTime;
    int failureDuration;
    int failureCount;

    Fault(Priority_t p, Facility &facility, int failureTime, int failureDuration) : Process(p), facility(facility),
                                                                                    failureTime(failureTime),
                                                                                    failureDuration(failureDuration) {}

private:
    void Behavior() {
        while (true) {
            //naplanujeme poruchu
            this->Wait(Exponential(this->failureTime));

            //zazanemename poruchu
            this->failureCount++;

            //zabereme zarizeni
            if (this->facility.Busy()) {
                this->Seize(this->facility, 1);
            } else {
                this->Seize(this->facility);
            }

            //pockame n sekund
            this->Wait(Exponential(failureDuration));

            //vratime zarizeni
            this->Release(this->facility);
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
        skeletonPalletStore.Enter(this, C_SKELETON_PALLET_STORE);
        resultPalletStore.Enter(this, C_RESULT_PALLET_STORE);
        metalSheetPalletStore.Enter(this, C_SHEET_METAL_PALLET_STORE);

        //bybereme ze skladu objednavky
        //kdyz prijde nova objednavka, volanim Leave se vlozi do skladu
        orders.Enter(this, C_ORDERS);

        hasPAndSPalet.Enter(this, 2);

        (new EmptyResultPalletWatcher())->Activate();
        (new EmptySkeletonPalletWatcher())->Activate();
        (new OrderGenerator())->Activate();
        //posleme delnika az za chvili, aby se stihlo vse inicializovat
        (new HumanWorker())->Activate(1);

        return; //RIP init
    }
};


int main() {
    SetOutput("main2.dat");
    srand(time(NULL));
    int random = rand();

    RandomSeed(random);
//    RandomSeed(2144570439);

    _Print("random seeed: %d\n\n\n", random);

    Init(0, 365 * DAY);

    (new SimulationInit())->Activate();

    //porucha PM cca 90 dni
    //opraveni potrva 24 hodin
    Fault *pm1Fault = new Fault(15, pm1, 90 * DAY, 24 * HOUR);
//    pm1Fault->Activate();

    //porucha pm1 buffer cca 30 dni
    //opraveni potrva 20 minut
    Fault *pmLoadingBufferFault = new Fault(15, pmLoadingBuffer, 30 * DAY, 20 * MINUTE);
//    pmLoadingBufferFault->Activate();

    //porucha pm1 buffer cca 30 dni
    //opraveni potrva 30 minut
    Fault *separatorFault = new Fault(15, separator, 30 * DAY, 30 * MINUTE);
//    separatorFault->Activate();

    Run();

    Print("Punching machine faults: %d\n", pm1Fault->failureCount);
    Print("PM loading buffer faults: %d\n", pmLoadingBufferFault->failureCount);
    Print("Separator faults: %d\n", separatorFault->failureCount);

    beforePmWorkerQueue.Output();
    beforePmWorker.Output();
    afterPmWorkerQueue.Output();
    afterPmWorker.Output();


    pmBuffer.Output();
    pmBufferQueue.Output();

    pmLoadingBuffer.Output();
    pmLoadingBufferQueue.Output();

    pm1.Output();
    pm1Queue.Output();

    pm2.Output();
    pm2Queue.Output();

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
    orders.Output();

    pocetPaletVZakazce.Output();
    delkaZpracovaniPlechu.Output();

}
