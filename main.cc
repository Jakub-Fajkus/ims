#include <iostream>

#include <simlib.h>

const int Sanitek = 2;

Facility Sanitka[Sanitek];
Queue	cekani("Cekani na sanitku");	// bude jedna fronta pro vsechny sanitky

Histogram hist("Doba stravena v systemu", 0, 5, 7);

class    Pacient : public Process {
public:
    Pacient(int pri) : Process(pri) {} ;

    void    Behavior() {
        double time = Time;

        int san;

        opak:

        san=-1; prerusen=0;

        // hleda volnou sanitku
        for (int a=0; a<Sanitek; a++)
            if (!Sanitka[a].Busy())
                san=a;

        if (san!=-1)
            Seize(Sanitka[san]); // nasel a bere ji
        else {
            // nenasel, uklada se do fronty
            cekani.Insert(this);
            Passivate();

            goto opak; // nekdo ho aktivuje, proces se tu vynori a skoci na zacatek
        }

        Wait(Uniform(1,5));
        if (prerusen) goto opak; // jestli byl prerusen, opakuje

        Wait(Exponential(1)); // nakladani pacienta
        if (prerusen) goto opak;

        Wait(Uniform(1,5));
        if (prerusen) goto opak;

        Release(Sanitka[san]);
        /*
            Release by mel vytahnout z fronty Q1 proces a ten dat do obsluhy. Q1 se ale nepouziva (a proto
            je ve statistikach prazdna).
            Aktivni proces delajici release "rucne" vytahne dalsiho z fronty a aktivuje ho.
            Aktivovany se okamzite spusti (naplanuje se na cas Time) a pokracuje v miste, kde se
            pasivoval
        */
        if (cekani.Length()>0) {
            (cekani.GetFirst())->Activate();
        }

        hist(Time-time);
    }

    int prerusen;
};



class   Generator : public Event {
public:
    Generator(double interv, int pri) : Event() {
        Interval = interv;
        Pri = pri;
    };

    void    Behavior() {
        (new Pacient(Pri))->Activate();
        Activate(Time+Exponential(Interval));
    }

    double  Interval;
    int     Pri;
} ;


class	Oprava : public Process {
public:
    // porucha se vztahuje na sanitku c. San
    Oprava(int san) : Process() , San(san) {};

    void Behavior() {
        Seize(Sanitka[San], 1); // zabrani sanitky s vyssi prioritou obsluhy (porucha)

        // pokud nebyl pred tim nikdo v obsluze, je Q2 prazdna!!, jinak:
        if (Sanitka[San].Q2->Length()>0) {
            // se z ni vyjme process
            Pacient *pac = (Pacient *)Sanitka[San].Q2->GetFirst();
            // nastavi se mu atribut "prerusen"
            pac->prerusen=1;
            // a aktivuje se....
            pac->Activate();

            /* proces "pac" byl pred tim naplanovan v kalendari (volal Wait(...)). Jeho odstaveni
            do fronty Q2 znamena, ze se zrusi jeho plan v kalendari a proces se pasivuje (neni mozno odhadnout,
            kdy se uvolni zarizeni a znovu se mu preda obsluha. Proto je vyrazen z kalendare, kde jsou
            naplanovany konkretni doby
            */
        }

        Wait(Exponential(10)); // probiha oprava
        Release(Sanitka[San]);

    }

    int San;
} ;


// udalost vyvolani poruchy
class	Porucha : public Event {
public:
    Porucha(int san) : Event() , San(san) {};

    void Behavior() {
        // porucha je tvorena nekolika kroky v case a musi proto byt implementovana procesem
        (new Oprava(San))->Activate();
        Activate(Time+Exponential(700));
    }

    int San;
} ;


int     main()
{
    SetOutput("sanitka.dat");
    int a;

    //for (a=0; a<Sanitek; a++)
    //	Sanitka[a].SetQueue(&cekani);

    Init(0, 1000);

    (new Generator(8,0))->Activate();

    // prvni porucha se naplanuje az na cas exp(700)
    for (a=0; a<Sanitek; a++)
        (new Porucha(a))->Activate(Exponential(700));

    Run();

    cekani.Output();
    hist.Output();

    for (a=0; a<Sanitek; a++)
        Sanitka[a].Output();
}
