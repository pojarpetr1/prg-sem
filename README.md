# Semestrální práce B3B36PRG

Projekt implementuje výpočet a vizualizaci **Juliovy množiny**. Aplikace je rozdělena na řídicí část běžící na PC a část běžící na vývojové desce Nucleo-F446RE.

Řídicí aplikace umožňuje nastavit parametry výpočtu, komunikovat s deskou Nucleo přes sériový port a průběžně zobrazovat dosud vypočítanou část fraktálu. Nucleo provádí samotný výpočet jednotlivých bodů Juliovy množiny a výsledky odesílá do řídicí aplikace.

Komunikace mezi PC a Nucleo pak probíhá pomocí vlastního komunikačního protokolu.

Aplikace využívá více vláken pro obsluhu uživatelského vstupu a sériové komunikace. Pro předávání zpráv mezi vlákny používá frontu a výsledný fraktál zobrazuje pomocí knihovny SDL.

