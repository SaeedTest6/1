# ادغام هسته‌ی Henge با vphone-cli — وضعیت

تاریخ: 2026-11-11 · روی لینوکس x86_64 نوشته شده؛ **روی آیفون چیزی اجرا نشده؛ روی رانر arm64 macOS تست‌های موتور پاس شدن (بخش Actions پایین).**

## معماری (بازبینی‌شده با اسکرین‌شات سازنده)

دو منبع درباره‌ی اپ اصلی داریم و با هم نمی‌خونن:

- **حرف سازنده:** vphone روی مک VM می‌سازه و اپ فقط اجراش می‌کنه («فعلاً»).
- **اسکرین‌شات اپ (اسمش Monolith):** صفحه‌ی Install با انتخاب دستگاه (Mac، HomePod mini، Apple Watch، Apple TV 4K)، نسخه («Boot chain from cloudOS 26.4»)، دکمه‌ی «Download and Install» و ۲۰ گیگابایت فضای خالی. مراحل روی خود آیفون: Download and prepare، Patch the boot chain، Restore (لاگ `ASR RESTORE PROGRESS`)، Install the system. یعنی **دانلود، پچ و ریستور روی خود آیفون انجام می‌شه**، دست‌کم در این نسخه.

اسکرین‌شات قوی‌تره، پس این برداشت قبلی من غلط بود: «ریستور و DFU برای آیفون لازم نیست». اگه ریستور روی دستگاه باشه، مسیر DFU/USB (`virtio_usb`، `run_installer`) و پشته‌ی ریستور Henge (idevicerestore، libirecovery، usbmuxd؛ README خود Henge هم می‌گه «بدون مک») لازم‌ان. ریستور رسیدن به ramdisk و `ramrod` رو هم نشون می‌ده، پس OpenAVB و SEP (دست‌کم لایه‌ی ROM) توی اون اپ جواب دادن. این‌که SEPOS رو چطور حل کرده معلوم نیست. نمی‌دونم کرنل‌کش واچ (که vphone با `kmutil` می‌سازه) رو روی گوشی چطور درمیاره.

فرضیه‌ی تازه: چون ریستور با موتور خودمون انجام می‌شه، نه با Virtualization.framework، می‌شه همین موتور رو روی رانر arm64 macOS در Actions اجرا کرد و VM رو بدون مک واقعی ساخت. فقط «ساخت با خود vphone» به مک فیزیکی نیاز داره. این رو هنوز امتحان نکردم.

جدول زیر مربوط به بسته‌ی VM در vphone است (برای مسیر «ساخت روی مک»):

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

نتیجه برای مسیر «ساخت روی مک»: DFU/USB لازم نیست. برای ریستور روی دستگاه (اسکرین‌شات بالا) لازمه و `force_dfu` همونه.

## این درخت چیه

`Engines/HengeEngine/` همون موتور Henge ـه (کامیت `0a1e6a5`، GPL-2.0): `core/` (مترجم arm64→arm64، MMU، استثناها)، `devices/`، `host/`، `tests/`. اپ آیفون و restore stack عمداً نیومدن. هر تغییر روی کد Henge یه کامیت جداست:

1. `Vendor Henge engine` — کپی دست‌نخورده.
2. `core: split 16-byte MMIO accesses` — موتور هر دسترسی MMIO بزرگ‌تر از ۸ بایت رو خطا می‌گرفت؛ صندوق‌پیام SEP با `ldp`/`stp` کار می‌کنه. حالا دو دسترسی ۸ بایتی (اول نیمه‌ی پایین). **کامپایل نشده** (به هدرهای Mach نیاز داره).
3. `devices: add the vresearch1 SEP mailbox model` — `sep_mailbox_core.[ch]`، `sep_mailbox.[ch]`، `vresearch.h`، تست.
4. `devices: add the vresearch1 board as a layer on vmapple` — `vresearch_platform.[ch]` (خالص) و `vresearch_board.[ch]` (لایه روی `hv_vmapple_create`).
5. `devices: add the SEP core's address space` — `sep_core.[ch]`: فضای آدرس ROM مربوط به OpenSEP روی یه `HvMachine` جدا، و بیدارکردن دو ماشین از روی صندوق‌پیام.

## چی فهمیدم

- **`core/` وابسته به پلتفرم نیست**؛ وابستگی به vmapple در `devices/` و `host/vm.c` ـه.
- **vresearch1 یه واریانت vmapple ـه.** آدرس ROM (`0x100000`)، بلوک پلتفرم (`0x400000`)، کنسول (`0x20010000`)، pvpanic (`0x20070000`)، bdif (`0x30000000`، با صف USB در `+0x100000`) و RAM (`0x70000000`) یکی‌ان؛ `_Static_assert` های `vresearch_board.c` این رو ثابت نگه می‌دارن.
- **بلوک پلتفرم:** چیدمان رجیسترهایی که ROM می‌خونه با `cfg.c` یکیه (فقط مقدار فرق داره: board ID در `cfg.c` برابر `0x20` ـه، ROM `0x90` می‌خواد). بنابراین `hv_vresearch_fill_platform` فقط مقدار می‌نویسه، چیزی جابه‌جا نمی‌شه. آفست‌ها با `tools/check_openavb_offsets.py` از روی سورس OpenAVB چک می‌شن.
- **تنظیم بوت:** ROM فقط وقتی بوت پلتفرم می‌کنه که boot config برابر ۱ یا ۲ باشه (`boot_policy.c`). `cfg.c` همین‌الان ۱ می‌ذاره.
- **کنسول** یه رجیستر نوشتنی ۳۲ بیتیه؛ PL011 هم جواب می‌ده.
- **SEP دو لایه داره، و فقط لایه‌ی اول مستند و قابل ساخته.**
  1. *ROM (OpenSEP):* پروتکل صندوق‌پیام، رمزگشایی و بررسی ایمیج، تحویل به فرمور. قرارداد کاملش توی `OpenSEP/docs/external-contract.md` هست و `sep_core.[ch]` همون فضای آدرس رو می‌سازه: بلوک دستگاه خالی ۳۲ مگابایتی در `0`، ROM در `0x02000000`، رم SEP در `0x04020000`، ناحیه‌ی runtime در `0x06000000` (اندازه‌اش رو AP با opcode `0x11` تعیین می‌کنه)، صندوق‌پیام در `0x30250000`، و رم AP در `0x70000000` به‌صورت اشتراکی (SEP فرمور رو از آدرسی که AP می‌ده می‌خونه).
  2. *SEPOS (فرمور اپل، `sep-firmware.vphone600`):* بعد از ROM همین روی هسته‌ی SEP اجرا می‌شه و به سخت‌افزاری نیاز داره که پروسه‌ی بسته‌ی SEP در Virtualization.framework می‌ده، از جمله ذخیره‌ی `SEPStorage` (۵۱۲ کیلوبایت: راز ۳۲ بایتی هر ماشین، رکوردهای A/B، رکوردهای anti-replay). **در هیچ‌کدوم از سورس‌هایی که دارم این دستگاه‌ها توصیف نشدن.** vphone هم SEP رو همیشه روشن می‌ذاره (حالت بدون SEP نداره). این بزرگ‌ترین ریسک کل پروژه‌ست؛ سازنده‌ی اپ اصلی یا اینو حل کرده یا راه دیگه‌ای رفته، و من نمی‌دونم کدوم.
- **SEP باید قبل از هر چیز آماده باشه.** در `src/protocol/sep/sep.c`، `sep_bootstrap()` قبل از انتخاب مسیر بوت/DFU اجرا می‌شه. درخواست BOOTSTRAP که جواب نگیره timeout می‌خوره و ROM `pvpanic_trigger()` صدا می‌زنه. **این نتیجه از خوندن سورس درآمده، نه از اجرا.** (نسخه‌ی قبلی این فایل «اول DFU، بعد SEP» می‌گفت؛ غلط بود.)

## چی انجام و تست شده

- مدل صندوق‌پیام: gcc + ASan/UBSan روی لینوکس، ۸ تست پاس (`tests/test_sep_mailbox.c`).
- بلوک پلتفرم: ۶ تست پاس با gcc + ASan/UBSan (`tests/test_vresearch_platform.c`)؛ یه جهش (board ID = `0x91`) تست رو به‌درستی شکست داد.
- `sep_core.c` یه تست چیدمان با ماشین stub داره (`tests/test_sep_core.c`، با ASan/UBSan؛ دو جهش عمدی گرفته شد). stub فقط ثبت می‌کنه چی خواسته شده، کد اجرا نمی‌کنه.
- `vresearch_board.c`، `sep_core.c` و `sep_mailbox.c` با هدرهای Henge روی x86_64 فقط type-check شدن (`gcc -fsyntax-only`). لینک نشدن، چون Henge به Mach/Metal نیاز داره.
- مدل صندوق‌پیام فقط از بیت‌هایی ساخته شده که OpenSEP/OpenAVB اسم بردن؛ با سخت‌افزار/Virtualization.framework مقایسه نشده. شماره‌ی وقفه‌ی صندوق‌پیام معلوم نیست (به نظر OpenAVB poll می‌کنه).
- `machine.c` (اصلاح ۱۶ بایتی) هنوز کامپایل نشده.

## کار باقی‌مونده (به ترتیب)

1. **هسته‌ی SEP، بخش ROM:** لایه‌ی دستگاه (`sep_core.[ch]`) آماده و با stub تست شده. هنوز مونده: ترد CPU برای ماشین دوم، و refactor در `host/vm.c` که تک‌نمونه‌ایه (`hv_vm_machine()` و `m` سراسری‌ان و handler سیگنال `on_ill` فقط ناحیه‌ی JIT همون یکی رو می‌شناسه). هر ماشین ناحیه‌ی JIT خودش رو داره (`hv_machine_init_jit(m, rx, rw, bytes)`)، پس SEP می‌تونه یه تکه‌ی کوچک از همون ناحیه‌ی ۱ گیگابایتی بگیره. `core/mmu.c` چند متغیر سراسری برای VA کرنل داره (`kva_*`) که اثرشون روی دو ماشین رو نسنجیدم. بدون این بخش، ROM همیشه پنیک می‌کنه.
   **هسته‌ی SEP، بخش SEPOS:** دستگاه‌های مورد نیاز فرمور اپل نامعلومن (بالا). قبل از نوشتن هر چیزی باید معلوم بشه SEPOS روی vresearch1 چه رجیسترهایی می‌خونه؛ راه‌های ممکن: ردگیری با GDB stub ـی که vphone برای SEP روشن می‌کنه، یا پرسیدن از سازنده.
2. **اولین بوت روی مک/آیفون:** `make` کامل روی مک، بعد بوت OpenAVB تا پیام `OpenBootROM: [sep] bootstrap configuration ready`. سؤال‌های باز: سازگاری GIC و تایمر Henge با ROM، رفتار PL011 با رجیستر کنسول.
3. **وارد کردن بسته‌ی VM:** خوندن `config.plist` (فرمت `VZMacMachineIdentifier` برای ECID رو ندیدم؛ باید از کد vphone استخراج بشه)، نگاشت `Disk.img`→root، `nvram.bin`→aux (تأیید کانال)، `SEPStorage`→SEP. اندازه‌ی `Disk.img` واچ و راه انتقالش به آیفون بررسی نشده.
4. **بوت تا iBoot و کرنل واچ**، نمایش و ورودی: `pvg_lite` فقط پروتکل GPU مجازی macOS رو بازپخش می‌کنه؛ سازگاری با درایور watchOS نامعلومه. لمس و دکمه‌ها از `vphoned` می‌گذرن (چرخش Crown راهی نداره).
5. **جفت‌سازی:** بدون آیفون مجازی دوم یا هوک رد‌کردن Setup، واچ از صفحه‌ی جفت‌سازی رد نمی‌شه.
6. **درز در Swift:** `VPhoneVirtualMachine.swift` به Virtualization.framework بسته است؛ برای اپ آیفون باید یه لایه‌ی backend بین UI و VM بیاد (شروع نشده). اپ آیفون ممکنه فقط از موتور C و UI جدید استفاده کنه، نه از Swiftِ vphone.

## Monolith: پروژه‌ی سازنده (بررسی‌شده، ۱۴۰۵/۸/۲۱)

مخزن: `MakrSas/Monolith`. فورک Henge با لایسنس GPL-2.0-or-later، پس هم‌لایسنس با درخت ماست.

- **SEP:** `host/sep.c` هسته‌ی SEP رو روی یه ماشین Henge جدا اجرا می‌کنه، با دستگاه‌هایی که ما نمی‌شناختیم: SEPStorage به اسم VSTR (`0x30020000`، شبیه virtio-blk) با بافر درخواست VSTD (`0x08000000`)، GIC جدا در `0x10000000`، TZ0 در `0x06000000` (۳۲ مگ)، SRAM ۸ مگ در `0x04000000`، صندوق پیام `iop-sep,vpiop` در `0x30250000` (`devices/vpiop.c`) و یه دستگاه ناشناخته در `0x30220000` که فقط لاگ می‌شه. مرجعش QEMU `hw/vmapple/vresearch101.c` و `sep-storage.c` است. این همون ریسک اصلی (SEPOS) بود.
- **واچ:** از vphone روی مک منتقل می‌شه (`tools/export_vphone.sh`). کراون و دکمه‌ی کناری زیر صفحه‌ان. نصب روی خود گوشی در `GuestKind` با یادداشت «Not yet: its system does not finish starting yet» بسته است، یعنی سیستم‌عامل واچ روی موتورش هنوز کامل بالا نمیاد.
- **نصب روی گوشی** برای HomePod mini و HomeView کار می‌کنه (دانلود از اپل، پچ بوت‌چین، ریستور در موتور خودش، APFS خوان/نویسنده‌ی خودش در `apfs/`).

نتیجه: هسته‌ی SEP خودمون تکراریه. مسیر درست: Monolith پایه بشه و کار روی بالا آمدن واچ‌او‌اس در موتورش متمرکز بشه. کار ما در `Engines/HengeEngine` (تست‌ها، ردگیر دسترسی‌های نگاشت‌نشده، CI) برای این دیباگ هنوز به درد می‌خوره.

## وضعیت نهایی (۱۴۰۵/۸/۲۳)

- **کار می‌کنه:** CI روی مک گیت‌هاب کل زنجیره را می‌سازد: دانلود فرمور Watch7,18 (watchOS 26.4)، پچ، ریستور و بوت روی موتور Monolith. بسته‌ی آماده‌ی ایمپورت برای اپ هم ساخته می‌شود (Watch workflow). IPA اپ با اسم Plutonium و آیکون Pu از `ipa.yml` ساخته می‌شود.
- **روی آیفون واقعی تست شد:** ساعت تا صفحه‌ی «Bring iPhone near Apple Watch» بالا آمد (گزارش کاربر).
- **بسته است:** عبور از این صفحه. ساعت مجازی بلوتوث ندارد؛ جفت‌شدن بین VMها با IKEv2 روی UDP است و Monolith هنوز آن را پیاده نکرده (docs/watch-pairing-notes.md و watch-pairing-terminusd.md). اتصال به اپ Watch آیفون واقعی شدنی نیست (BLE و فعال‌سازی اپل).
- **قدم بعدی اگر ادامه دهیم:** پیاده‌سازی نقش «آیفون» در اپ (OOBK و IKEv2). نیاز به مک و یک VM آیفون برای مشاهده‌ی ترافیک دارد و در CI تنها قابل انجام نیست.

## ساخت و تست با GitHub Actions (بدون مک)

- `.github/workflows/engine.yml` روی رانر `xcode-27` (arm64، macOS 27) اجرا می‌شه: `make check-engine` همه‌ی تست‌های موتور رو می‌سازه و اجرا می‌کنه، از جمله `tests/test_vresearch.c` که روی موتور واقعی یه برنامه‌ی مهمان با `ldp`/`stp` روی صندوق‌پیام SEP و خوندن بلوک پلتفرم اجرا می‌کنه. این تنها جاییه که `machine.c` و اصلاح ۱۶ بایتی واقعاً کامپایل و اجرا می‌شن. **اجرا شد (GitHub Actions، `xcode-27`): همه‌ی تست‌های موتور پاس شدن، از جمله `test_vresearch` با ۱۳ چک، یعنی `ldp`/`stp` ۱۶ بایتی روی صندوق‌پیام و خوندن هویت بلوک پلتفرم روی موتور واقعی کار می‌کنه. چک آفست‌ها با OpenAVB هم پاس شد.**
- رانر `xcode-27` برای ریپوی عمومی رایگانه؛ روی ریپوی خصوصی دقیقه‌های macOS ده برابر حساب می‌شن (طبق توضیح workflow خود Henge).
- **ساخت VM با خود vphone روی Actions ممکن نیست (ریستور با موتور خودمون شاید ممکن باشه، بالا رو ببین):** vphone فقط روی مک فیزیکی Apple Silicon کار می‌کنه و باید توی Recovery دستور `csrutil allow-research-guests enable` زده بشه؛ رانرها خودشون VM ان و Recovery ندارن. پس خود VM باید یه‌بار روی یه مک واقعی (یا مک اجاره‌ای) ساخته بشه.
- **راه SEPOS با Actions:** رانر اینترنت داره، پس می‌تونه فرمور cloudOS رو از CDN اپل بگیره و `sep-firmware` رو بیرون بکشه (فقط موقت، هرگز commit یا منتشر نمی‌شه). بعد SEPOS رو توی موتور اجرا می‌کنیم با یه ردگیر که هر دسترسی به آدرس نگاشت‌نشده رو لاگ کنه و از روی لاگ دستگاه‌ها رو یکی‌یکی مدل می‌کنیم. این حلقه‌ی ویرایش/اجرا/لاگ بدون مک ممکنه.

## اولین بوت (GitHub Actions، اجرای واقعی)

`host/vresearch_run.c` و workflow به اسم Boot: OpenAVB و OpenSEP از سورس ساخته می‌شن و روی موتور ما، هر کدوم روی یه `HvMachine` و ترد جدا، بوت می‌شن.

- **AP تنها (بدون SEP):** OpenAVB از اولین اجرا بوت شد: `[boot] runtime initialization` → `[sep] bootstrap begin` → `SEP RX timeout` به‌صورت حلقه. همون چیزی که از خوندن سورس پیش‌بینی کرده بودم.
- **با هسته‌ی SEP:** اول SEP روی `pc 0x200` مرد (exit 9). با لاگ `unmapped write: pa 4010028 pc 200004c` معلوم شد اولین دستور ROM (`start.S`) یه breadcrumb به `0x04010028` می‌نویسه، پایین‌تر از RAM مستند. یه صفحه RAM در `0x04010000` اضافه شد (`SEPCORE_SCRATCH_*`).
- **بعد از اصلاح:** `OpenBootROM: SEP bootstrap complete`، بعد `virtual firmware source not found` → `falling back to DFU` → `[dfu] ready for image upload`. یعنی **ROM اپل-مستقل (OpenAVB) و ROM مربوط به SEP (OpenSEP) روی موتور Henge با هم حرف می‌زنن و ROM به DFU می‌رسه.** SEP هم روی `pc 0x2019a30` در EL1 با MMU روشن در حال اجراست.
- هنوز نه دیسک داریم، نه فرمور، نه SEPOS. DFU رسیدن یعنی آماده‌ی مرحله‌ی بعد: ریستور (آپلود iBSS و ...) از مسیر `virtio_usb`.

## بهینه‌سازی برای iOS 27

اپ آیفون هنوز نیومده، پس این‌ها تصمیم‌های طراحی‌ان، نه کد. چیزی از این‌ها روی iOS 27 اجرا نشده.

- **هدف نسخه:** Henge با `deploymentTarget` برابر iOS 17.0 و Xcode 27 ساخته می‌شه و مسیرهای جدا برای iOS 17–26 داره (نمایش خارجی قدیمی `LegacyExternalDisplay`، JIT قبل از TXM). برای اپ جدید پیشنهاد: فقط iOS 27.0، که این مسیرهای قدیمی رو کنار می‌ذاره.
- **JIT:** از iOS 26 به بعد (TXM) حافظه‌ی اجرایی فقط با دیباگر متصل داده می‌شه. Henge یه کمکی داخلی StikJIT داره (`HengeJITHelper.appex`) که روی iOS 27 جفت‌سازی روی خود دستگاه داره. entitlementها: `get-task-allow` همیشه؛ `increased-memory-limit` و `extended-virtual-addressing` با تیم شخصی (رایگان) امضا نمی‌شن، و Henge یه فایل entitlements کم‌تر برای اون حالت داره.
- **حافظه:** واچ در vphone با ۴۰۹۶ مگابایت تست شده (قبلاً ۲۰۴۸). بودجه = رم AP + ناحیه‌ی کوچک SEP + ناحیه‌ی JIT یک گیگابایتی. گزارش‌های جامعه (نه اپل) می‌گن iOS 27 بتا ۳ سقف رم هر اپ رو روی دستگاه‌های ۱۲ گیگی تا حدود ۸.۶ گیگ بالا برده؛ رسمی نیست و معلوم نیست در نسخه‌ی نهایی بمونه. پس سقف ثابت ننویس؛ اندازه‌ی رم مهمان رو موقع اجرا از حافظه‌ی در دسترس بگیر (Henge خودش مهمان رو به اندازه‌ی «چیزی که می‌مونه» می‌کنه).
- **نمایش خارجی:** `ExternalNonInteractiveAccessory` فقط روی iOS 27 هست؛ Henge اون رو برای نشست مک ثبت می‌کنه. برای واچ لازم نیست.
- **ساخت:** Xcode 27 یا جدیدتر.

## لایسنس

هر چیزی که به کد Henge لینک بشه باید GPL-2.0 بمونه. vphone-cli (MIT) رو می‌شه توش گنجوند، ولی ترکیب نهایی GPL ـه. OpenAVB/OpenSEP هم MIT ان.

## چطور روی vphone-cli بذارمش

پوشه‌ی `Engines/` رو کنار ریشه‌ی چک‌اوت vphone-cli کپی کن؛ به هیچ فایلی از vphone دست نمی‌زنه. تست‌های خالص روی مک یا لینوکس:

```
cd Engines/HengeEngine
make build/test_sep_mailbox build/test_vresearch_platform build/test_sep_core
build/test_sep_mailbox && build/test_vresearch_platform && build/test_sep_core
python3 tools/check_openavb_offsets.py /path/to/OpenAVB
```
