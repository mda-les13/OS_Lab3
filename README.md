# Лабораторная работа №3  
**Тема:** «Синхронизация потоков при помощи критических секций и событий. Обработка тупиков»   

## Задача  
Разработать консольную программу, состоящую из одного потока **main** и нескольких экземпляров потока **marker**, реализующую:  
- Синхронизацию потоков с использованием критических секций и событий.  
- Обработку тупиков при конкурентном доступе к общему ресурсу.  

---

## Функционал потоков  

### **main**  
1. Создает массив целых чисел:  
   - Размерность массива вводится пользователем через консоль.  
2. Инициализирует все элементы массива нулями.  
3. Запрашивает количество потоков **marker** для запуска.  
4. Запускает указанный объем экземпляров потока **marker**, передавая каждому его порядковый номер.  
5. Подает сигнал всем потокам **marker** для начала работы.  
6. Выполняет циклические действия:  
   1. Ждет, пока все потоки **marker** не сообщат о невозможности продолжения.  
   2. Выводит текущее содержимое массива на консоль.  
   3. Запрашивает номер потока **marker**, которому требуется завершить работу.  
   4. Отправляет сигнал на завершение указанному потоку.  
   5. Ожидает завершения работы этого потока.  
   6. Выводит обновленное содержимое массива.  
   7. Подает сигнал о продолжении оставшимся потокам **marker**.  
7. Завершает программу после окончания работы всех потоков **marker**.  

---

### **marker**  
1. Начинает выполнение по сигналу от потока **main**.  
2. Инициализирует генератор случайных чисел через `srand(номер_потока)`.  
3. Работает циклически:  
   1. Генерирует случайное число с помощью `rand()`.  
   2. Вычисляет индекс в массиве: `index = rand() % размерность_массива`.  
   3. Если `массив[index] == 0`:  
      - Вызывает `Sleep(5)` (пауза 5 мс).  
      - Записывает свой порядковый номер в `массив[index]`.  
      - Снова вызывает `Sleep(5)`.  
   4. Если `массив[index] != 0`:  
      - Выводит на консоль:  
        - Свой номер.  
        - Количество помеченных элементов.  
        - Индекс непомеченного элемента.  
      - Отправляет сигнал потоку **main** о блокировке.  
      - Ожидает ответный сигнал на продолжение или завершение.  
4. При получении сигнала на завершение:  
   - Освобождает занятые элементы массива (устанавливает их в 0).  
   - Завершает работу.  
5. При получении сигнала на продолжение: возвращается к пункту 3.  
