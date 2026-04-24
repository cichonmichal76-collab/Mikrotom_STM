# Pakiety wdrozeniowe Wariant B

Ten dokument wyjasnia, ktora paczke ZIP nalezy pobrac i do czego ona sluzy.

## 1. Paczki do instalacji

| Paczka | Do czego sluzy | Kiedy pobrac |
| --- | --- | --- |
| `Mikrotom_Wariant_B_Pakiet_PC_20260424.zip` | Agent, GUI, API, SQL, skrypty uruchomieniowe, dokumentacja | na komputer podlaczony do USB-UART |
| `Mikrotom_Wariant_B_Pakiet_MCU_SAFE_20260424.zip` | firmware `SAFE` i skrypty flashowania | jako pierwszy flash MCU |
| `Mikrotom_Wariant_B_Pakiet_MCU_MOTION_20260424.zip` | firmware `MOTION-ENABLED` i skrypty flashowania | dopiero po zaliczeniu `SAFE` |
| `Mikrotom_Wariant_B_Pakiet_Pelny_20260424.zip` | wszystko razem w jednej paczce | gdy chcesz miec komplet w jednym archiwum |

## 2. Paczka dokumentacji

| Paczka | Zawartosc |
| --- | --- |
| `Mikrotom_Wariant_B_Pakiet_Dokumentacja_20260424.zip` | instrukcja instalacji, instrukcja GUI, checklista bring-up, przewodnik HTML, karta operatora A4, zrzuty ekranow |

## 3. Kolejnosc pobierania dla laika

Najbezpieczniejsza kolejnosc:

1. pobierz `Pakiet Dokumentacja`,
2. pobierz `Pakiet PC`,
3. pobierz `Pakiet MCU SAFE`,
4. po zaliczeniu Etapu 1 i 2 pobierz albo wykorzystaj `Pakiet MCU MOTION`.

## 4. Gdy chcesz wszystko w jednym

Jesli nie chcesz pilnowac kilku archiwow, pobierz:

- `Mikrotom_Wariant_B_Pakiet_Pelny_20260424.zip`

W tej paczce masz:

- GUI,
- Agenta,
- skrypty `.bat`,
- firmware `SAFE`,
- firmware `MOTION-ENABLED`,
- dokumentacje.
