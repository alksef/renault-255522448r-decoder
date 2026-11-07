# Нумерация фотографии

Использован встроенный ImageGen, режим редактирования по исходной фотографии.
Результат: `images/connector-numbered.png`; неизменённый исходник:
`images/connector-reference.png`. Исходный файл предоставлен пользователем
из [статьи на DRIVE2](https://www.drive2.ru/l/6899697/), не является его собственной фотографией.

Задание для изображения: сохранить фотографию, ориентацию и геометрию разъёма;
нанести жёлтые номера с чёрной обводкой и короткими выносками на шесть контактов.
Верхний ряд слева направо 1, 2, 3; нижний слева направо 4, 5, 6.
Не зеркалить и не добавлять контакты. После генерации проверены порядок
номеров и попадание выносок в соответствующие контакты.

Полный использованный prompt:

> Edit target: supplied photograph of a Renault multimedia stalk on a laptop. Make a precise technical annotation ONLY; preserve the original photo, its framing, all geometry, connector and background. There are six metal connector contacts inside the rectangular socket at the left of the stalk, two rows of three. Top row contact centers at original 960x720 pixels approximately (220,357),(245,360),(270,361). Lower row contact centers approximately (220,393),(244,394),(270,394). Add six crisp high contrast numbered labels with short fine leader lines terminating at exactly those contacts: TOP ROW left-to-right 1, 2, 3; BOTTOM ROW left-to-right 4, 5, 6. Put labels 1 2 3 just above the top row and 4 5 6 just below the lower row. Do not obscure the contacts. Use yellow digits with black outline, modest size ~20px at original resolution. Do not invent or change any contacts, do not flip or mirror image. Add no extra objects or text. This is a pinout reference: exact numbering and unchanged geometry are essential.
