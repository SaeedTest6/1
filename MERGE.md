# ادغام هسته‌ی Henge با vphone-cli — وضعیت

تاریخ: 2026-11-11 · روی لینوکس x86_64 نوشته شده؛ **هیچ چیزی روی مک، آیفون یا arm64 اجرا نشده.**

## معماری (بر اساس حرف سازنده‌ی اپ اصلی)

سازنده نوشته: «تا حدی؛ vphone برای ساخت VM استفاده می‌شه، بعد اون رو روی گوشی بار می‌کنیم و با یه امولاتور اجراش می‌کنیم.» یعنی:

- **روی مک (vphone-cli):** دانلود ایمیج از سرور اپل، امضای TSS، ساخت کرنل‌کش (`kmutil`)، ریستور، پچ بوت‌چین. خروجی یه بسته‌ی VM ـه.
- **روی آیفون (موتور Henge + این برد):** فقط همون VM آماده رو بوت می‌کنه.

پس دانلود و نصب خودکار *روی خود آیفون* چیزی نیست که اون سازنده کرده باشه. موانعِ «کرنل‌کش با kmutil» و «امضای TSS روی آیفون» از مسیر ما برداشته می‌شن؛ این‌ها فقط روی مک لازم‌ان.

بسته‌ی VM در vphone (`~/.vphone/machines/<name>/`، از سورس و مستندات vphone خونده شد):

| فایل | کارش | معادل در برد |
|---|---|---|
| `Disk.img` | دیسک سیستم (`VZVirtioBlockDevice`) | دیسک root روی bdif |
| `nvram.bin` | `VZMacAuxiliaryStorage`؛ بوت‌چین بانک‌های NVRAM خودش رو اینجا می‌خونه | دیسک aux (**نیاز به تأیید**: ندیدم کدوم کانال bdif) |
| `SEPStorage` | ذخیره‌ی SEP (`_VZSEPCoprocessorConfiguration(storageURL:)`) | حافظه‌ی هسته‌ی SEP |
| `config.plist` | شناسه‌ی ماشین (ECID توش کد شده)، MAC، تنظیمات | `ecid`/`mac` در `hv_vresearch_create` |
| ROMها | OpenAVB و OpenSEP (توی `VPhone.bundle`) | `hv_vresearch_load_rom` و ROM هسته‌ی SEP |

مستندات vphone می‌گن عوض‌کردن ROM به OpenAVB/OpenSEP «بدون ریستور» و از بوت بعدی اعمال می‌شه، و هر دو ROM همون LLB پچ‌شده و همون فرمور SEP نصب‌شده رو لود می‌کنن. پس همون بسته‌ای که مک ساخته، برای موتور ما هم قابل بوته (نظری؛ اجرا نشده).
`Disk.img` و `SEPStorage` و `nvram.bin` باید با هم جابه‌جا بشن؛ ترکیب نامتناسب باعث پنیک SEP می‌شه (هشدار خود vphone).

نتیجه: مسیر DFU/USB-device (`virtio_usb`، `run_installer`) برای آیفون لازم نیست. فقط برای تست اولیه نگهش داشتم (`force_dfu`).

## این درخت چیه

`Engines/HengeEngine/` همون موتور Henge ـه (کامیت `0a1e6a5`، GPL-2.0): `core/` (مترجم arm64→arm64، MMU، استثناها)، `devices/`، `host/`، `tests/`. اپ آیفون و restore stack عمداً نیومدن. هر تغییر روی کد Henge یه کامیت جداست:

1. `Vendor Henge engine` — کپی دست‌نخورده.
2. `core: split 16-byte MMIO accesses` — موتور هر دسترسی MMIO بزرگ‌تر از ۸ بایت رو خطا می‌گرفت؛ صندوق‌پیام SEP با `ldp`/`stp` کار می‌کنه. حالا دو دسترسی ۸ بایتی (اول نیمه‌ی پایین). **کامپایل نشده** (به هدرهای Mach نیاز داره).
3. `devices: add the vresearch1 SEP mailbox model` — `sep_mailbox_core.[ch]`، `sep_mailbox.[ch]`، `vresearch.h`، تست.
4. `devices: add the vresearch1 board as a layer on vmapple` — `vresearch_platform.[ch]` (خالص) و `vresearch_board.[ch]` (لایه روی `hv_vmapple_create`).

## چی فهمیدم

- **`core/` وابسته به پلتفرم نیست**؛ وابستگی به vmapple در `devices/` و `host/vm.c` ـه.
- **vresearch1 یه واریانت vmapple ـه.** آدرس ROM (`0x100000`)، بلوک پلتفرم (`0x400000`)، کنسول (`0x20010000`)، pvpanic (`0x20070000`)، bdif (`0x30000000`، با صف USB در `+0x100000`) و RAM (`0x70000000`) یکی‌ان؛ `_Static_assert` های `vresearch_board.c` این رو ثابت نگه می‌دارن.
- **بلوک پلتفرم:** چیدمان رجیسترهایی که ROM می‌خونه با `cfg.c` یکیه (فقط مقدار فرق داره: board ID در `cfg.c` برابر `0x20` ـه، ROM `0x90` می‌خواد). بنابراین `hv_vresearch_fill_platform` فقط مقدار می‌نویسه، چیزی جابه‌جا نمی‌شه. آفست‌ها با `tools/check_openavb_offsets.py` از روی سورس OpenAVB چک می‌شن.
- **تنظیم بوت:** ROM فقط وقتی بوت پلتفرم می‌کنه که boot config برابر ۱ یا ۲ باشه (`boot_policy.c`). `cfg.c` همین‌الان ۱ می‌ذاره.
- **کنسول** یه رجیستر نوشتنی ۳۲ بیتیه؛ PL011 هم جواب می‌ده.
- **SEP باید قبل از هر چیز آماده باشه.** در `src/protocol/sep/sep.c`، `sep_bootstrap()` قبل از انتخاب مسیر بوت/DFU اجرا می‌شه. درخواست BOOTSTRAP که جواب نگیره timeout می‌خوره و ROM `pvpanic_trigger()` صدا می‌زنه. **این نتیجه از خوندن سورس درآمده، نه از اجرا.** (نسخه‌ی قبلی این فایل «اول DFU، بعد SEP» می‌گفت؛ غلط بود.)

## چی انجام و تست شده

- مدل صندوق‌پیام: gcc + ASan/UBSan روی لینوکس، ۸ تست پاس (`tests/test_sep_mailbox.c`).
- بلوک پلتفرم: ۶ تست پاس با gcc + ASan/UBSan (`tests/test_vresearch_platform.c`)؛ یه جهش (board ID = `0x91`) تست رو به‌درستی شکست داد.
- `vresearch_board.c` و `sep_mailbox.c` با هدرهای Henge روی x86_64 فقط type-check شدن (`gcc -fsyntax-only`). لینک نشدن، چون Henge به Mach/Metal نیاز داره.
- مدل صندوق‌پیام فقط از بیت‌هایی ساخته شده که OpenSEP/OpenAVB اسم بردن؛ با سخت‌افزار/Virtualization.framework مقایسه نشده. شماره‌ی وقفه‌ی صندوق‌پیام معلوم نیست (به نظر OpenAVB poll می‌کنه).
- `machine.c` (اصلاح ۱۶ بایتی) هنوز کامپایل نشده.

## کار باقی‌مونده (به ترتیب)

1. **هسته‌ی SEP:** یه `HvMachine` دوم با فضای آدرس خودش (OpenSEP در `0x02000000`، RAM در `0x04020000`، runtime در `0x06000000`) که سمت `HV_SEPMBX_SEP` رو وصل می‌کنه و `SEPStorage` رو به‌عنوان حافظه‌اش می‌گیره. `host/vm.c` تک‌نمونه‌ایه (`hv_vm_machine()` سراسریه)، پس باید refactor بشه. بدون این، ROM همیشه پنیک می‌کنه.
2. **اولین بوت روی مک/آیفون:** `make` کامل روی مک، بعد بوت OpenAVB تا پیام `OpenBootROM: [sep] bootstrap configuration ready`. سؤال‌های باز: سازگاری GIC و تایمر Henge با ROM، رفتار PL011 با رجیستر کنسول.
3. **وارد کردن بسته‌ی VM:** خوندن `config.plist` (فرمت `VZMacMachineIdentifier` برای ECID رو ندیدم؛ باید از کد vphone استخراج بشه)، نگاشت `Disk.img`→root، `nvram.bin`→aux (تأیید کانال)، `SEPStorage`→SEP. اندازه‌ی `Disk.img` واچ و راه انتقالش به آیفون بررسی نشده.
4. **بوت تا iBoot و کرنل واچ**، نمایش و ورودی: `pvg_lite` فقط پروتکل GPU مجازی macOS رو بازپخش می‌کنه؛ سازگاری با درایور watchOS نامعلومه. لمس و دکمه‌ها از `vphoned` می‌گذرن (چرخش Crown راهی نداره).
5. **جفت‌سازی:** بدون آیفون مجازی دوم یا هوک رد‌کردن Setup، واچ از صفحه‌ی جفت‌سازی رد نمی‌شه.
6. **درز در Swift:** `VPhoneVirtualMachine.swift` به Virtualization.framework بسته است؛ برای اپ آیفون باید یه لایه‌ی backend بین UI و VM بیاد (شروع نشده). اپ آیفون ممکنه فقط از موتور C و UI جدید استفاده کنه، نه از Swiftِ vphone.

## لایسنس

هر چیزی که به کد Henge لینک بشه باید GPL-2.0 بمونه. vphone-cli (MIT) رو می‌شه توش گنجوند، ولی ترکیب نهایی GPL ـه. OpenAVB/OpenSEP هم MIT ان.

## چطور روی vphone-cli بذارمش

پوشه‌ی `Engines/` رو کنار ریشه‌ی چک‌اوت vphone-cli کپی کن؛ به هیچ فایلی از vphone دست نمی‌زنه. تست‌های خالص روی مک یا لینوکس:

```
cd Engines/HengeEngine
make build/test_sep_mailbox build/test_vresearch_platform
build/test_sep_mailbox && build/test_vresearch_platform
python3 tools/check_openavb_offsets.py /path/to/OpenAVB
```
