# ادغام هسته‌ی Henge با vphone-cli — وضعیت

تاریخ: 2026-11-10 · روی لینوکس x86_64 نوشته شده؛ **هیچ چیزی روی مک، آیفون یا arm64 اجرا نشده.**

## این درخت چیه

`Engines/HengeEngine/` همون موتور Henge ـه (کامیت `0a1e6a5`، لایسنس GPL-2.0): `core/` (مترجم arm64→arm64، MMU، استثناها)، `devices/`، `host/`، `tests/`. اپ آیفون و restore stack عمداً نیومدن.
هر تغییر روی کد Henge یه کامیت جداست:

1. `Vendor Henge engine` — کپی دست‌نخورده.
2. `core: split 16-byte MMIO accesses` — موتور هر دسترسی MMIO بزرگ‌تر از ۸ بایت رو خطا می‌گرفت. صندوق‌پیام SEP با یه `ldp` خونده و با یه `stp` نوشته می‌شه، پس دسترسی ۱۶ بایتی حالا به دو دسترسی ۸ بایتی (اول نیمه‌ی پایین) شکسته می‌شه. **کامپایل نشده** (به هدرهای Mach نیاز داره)، فقط با چشم بازبینی شده.
3. `devices: add the vresearch1 SEP mailbox model` — `sep_mailbox_core.[ch]`، چسب `sep_mailbox.[ch]`، `vresearch.h` و تست.

## چی فهمیدم

- **موتور (`core/`) وابسته به پلتفرم نیست.** وابستگی به vmapple در `devices/` و `host/vm.c` ـه.
- **برد vresearch1 (همونی که vphone روی Virtualization.framework ازش استفاده می‌کنه) نزدیک vmapple ـه.** طبق قرارداد میزبان OpenAVB (`docs/external-contract.md`): ROM در `0x100000`، بلوک رجیستر پلتفرم در `0x400000`، کنسول در `0x20010000`، pvpanic در `0x20070000`، بلاک دیوایس‌ها و USB روی `0x30000000` (همون چیدمان `bdif` ـی که Henge دارد) و RAM در `0x70000000`. جدول مقایسه توی `devices/vresearch.h` ـه.
- **چیزی که vmapple نداره:** صندوق‌پیام SEP در `0x30250000` و یه هسته‌ی SEP با فضای آدرس جدا (OpenSEP.bin در `0x02000000`، RAM در `0x04020000`، runtime در `0x06000000`).
- OpenAVB و OpenSEP هر دو متن‌باز (MIT) و برای اجرا روی میزبان‌هایی غیر از Virtualization.framework (مثل QEMU) طراحی شدن. vphone هم باهاشون بوت می‌کنه.

## چی انجام و تست شده

- مدل صندوق‌پیام (`sep_mailbox_core.c`) با gcc و با ASan/UBSan روی لینوکس: تست‌ها پاس می‌شن (`tests/test_sep_mailbox.c`).
- چسب `sep_mailbox.c` با هدرهای Henge روی هدف x86_64 type-check می‌شه.
- مدل فقط از روی بیت‌هایی ساخته شده که OpenSEP و OpenAVB اسم بردن. با مدل واقعی اپل مقایسه نشده. بیت‌های بدون مستند خونده‌شده صفرن.

## کار باقی‌مونده (به ترتیب)

1. **برد `vresearch`** کنار `vmapple.c`: همون دستگاه‌ها به‌اضافه‌ی صندوق‌پیام. سؤال‌های بازش: رجیسترهای کنسول (آیا PL011 ـه یا فقط یه رجیستر داده)، چیدمان بلوک `0x400000` در برابر `cfg.c`، شماره‌ی وقفه‌ی صندوق‌پیام. جوابشون توی DeviceTree فرمور هست (`DeviceTree.vphone600ap`)؛ این‌جا به CDN اپل دسترسی نبود.
2. **هدف اول:** بوت OpenAVB تا چاپ `OpenBootROM:` روی کنسول و رسیدن به DFU (Henge مسیر DFU/restore رو برای macOS داره).
3. **هسته‌ی SEP:** یه `HvMachine` دوم با فضای آدرس خودش. `host/vm.c` تک‌نمونه‌ایه (`hv_vm_machine()` سراسریه)، پس باید refactor بشه. OpenSEP قبلاً روی QEMU هم اجرا شده، یعنی کدش روی مترجم arm64 معمولی می‌چرخه.
4. **ریستور و ایمیج واچ** با خط لوله‌ی `fw prepare/patch` ـی vphone (Swift، MIT). مرحله‌ی `kmutil` روی آیفون نیست.
5. **نمایش و ورودی:** `pvg_lite` فقط پروتکل GPU مجازی macOS رو بازپخش می‌کنه؛ سازگاری با درایور iOS/watchOS نامعلومه. لمس و دکمه‌ها از `vphoned` می‌گذرن.
6. **درز در Swift:** `VPhoneVirtualMachine.swift` الان کاملاً به Virtualization.framework بسته است (`VZMacMachineIdentifier` و...). باید یه لایه‌ی backend بین UI و VM بیاد. این کار هنوز شروع نشده.

## لایسنس

هر چیزی که به کد Henge لینک بشه باید GPL-2.0 بمونه. vphone-cli (MIT) رو می‌شه توش گنجوند، ولی ترکیب نهایی GPL ـه.

## چطور روی vphone-cli بذارمش

پوشه‌ی `Engines/` رو کنار ریشه‌ی چک‌اوت vphone-cli کپی کن. به هیچ فایلی از vphone دست نمی‌زنه. تست روی مک: `cd Engines/HengeEngine && make build/test_sep_mailbox && build/test_sep_mailbox`.
