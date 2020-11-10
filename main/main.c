/*
 * Author: Adrian Vazquez
 * Date: 10-07-2020
 *
 * Collab: Marco Rodríguez
 */

#include <stdio.h>
#include "CRC.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_rf1276.h"
#include "esp_task_wdt.h"
#include "flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "modbus_lora.h"
#include "modbus_master.h"
#include "modbus_slave.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "rf1276.h"
#include "strings.h"

#define PULSES_KW 225

#define RX_BUF_SIZE 1024
#define TX_BUF_SIZE 1024

#define TIME_SCAN 2000
#define MAX_SLAVES 255

#define TWDT_TIMEOUT_S 10

// Medidor de pulsos
#define STORAGE_NAMESPACE "storage"
#define PPE_LIMIT 10  // Pulses per entry
#define EPP_LIMIT 4   // Entries per page
#define PPP_LIMIT 3   // Page per partition
#define PARTITIONS_MAX 3

#define CHECK_ERROR_CODE(returned, expected) \
  ({                                         \
    if (returned != expected) {              \
      printf("TWDT ERROR\n");                \
      abort();                               \
    }                                        \
  })
static char *TAG = "INFO";
static uint16_t *modbus_registers[4];
static uint16_t inputRegister[512] = {0};

// METODOS
esp_err_t guarda_cuenta_pulsos(char *partition_name, char *page_namespace,
                               char *entry_key, int32_t *Pulsos) {
  nvs_handle_t my_handle;

  // Abriendo la página variable
  esp_err_t err = nvs_open_from_partition(partition_name, page_namespace,
                                          NVS_READWRITE, &my_handle);
  if (err != ESP_OK) return err;
  // Write
  err = nvs_set_i32(my_handle, entry_key, *Pulsos);
  // Commit
  err = nvs_commit(my_handle);
  if (err != ESP_OK) return err;
  // Close
  nvs_close(my_handle);

  return ESP_OK;
}

esp_err_t activa_entrada(char *partition_name, char *page_namespace,
                         char **entry_key, int32_t *actual_entry_index,
                         int32_t *Pulsos) {
  nvs_handle_t my_handle;

  free(*entry_key);
  if (asprintf(entry_key, "e%d", *actual_entry_index) <
      0) {  // Aquí se crea el keyvalue
    free(*entry_key);
    return ESP_FAIL;
  }

  // Abriendo la página variable
  esp_err_t err = nvs_open_from_partition(partition_name, page_namespace,
                                          NVS_READWRITE, &my_handle);
  if (err != ESP_OK) return err;
  // Write
  err = nvs_set_i32(my_handle, *entry_key, *Pulsos);
  // Commit
  err = nvs_commit(my_handle);
  if (err != ESP_OK) return err;
  // Close
  nvs_close(my_handle);

  return ESP_OK;
}

esp_err_t abrir_pv(char *partition_name, char **page_namespace,
                   char **entry_key, int32_t *actual_pv_counter,
                   int32_t *actual_entry_index, int32_t *Pulsos) {
  nvs_handle_t my_handle;

  esp_err_t err = nvs_open_from_partition(partition_name, STORAGE_NAMESPACE,
                                          NVS_READWRITE, &my_handle);
  if (err != ESP_OK) return err;
  // Write
  nvs_set_i32(my_handle, "pf", *actual_pv_counter);
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) return err;
  // Commit
  err = nvs_commit(my_handle);
  if (err != ESP_OK) return err;
  // Close
  nvs_close(my_handle);

  free(*page_namespace);
  if (asprintf(page_namespace, "pv%d", *actual_pv_counter) <
      0) {  // Aquí se crea el namespace para abrir la página
    free(*page_namespace);
    return ESP_FAIL;
  }

  activa_entrada(partition_name, *page_namespace, entry_key, actual_entry_index,
                 Pulsos);

  return ESP_OK;
}

esp_err_t leer_contador_pf(char **pname, char **pvActual,
                           int32_t *ContadorPaginaFija) {
  // Esta función lee el contador desde la pagina fija por defecto llamada
  // "storage", y se retorna a la ejecución del programa

  nvs_handle_t my_handle;
  esp_err_t err;

  ESP_LOGW("DEBUG LCPF", "%s", *pname);

  // Open
  err = nvs_open_from_partition(*pname, "storage", NVS_READWRITE, &my_handle);
  if (err != ESP_OK) return err;

  // Read
  err = nvs_get_i32(my_handle, "pf", ContadorPaginaFija);
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    return err;
  else if (err == ESP_ERR_NVS_NOT_FOUND) {
    *ContadorPaginaFija = 1;  // Valor por defecto en lugar de no estar asignado

    // Write
    err = nvs_set_i32(my_handle, "pf", *ContadorPaginaFija);

    // Commit
    err = nvs_commit(my_handle);
    if (err != ESP_OK) return err;
  }

  // Close
  nvs_close(my_handle);
  if (asprintf(pvActual, "pv%d", *ContadorPaginaFija) <
      0) {  // Aquí se crea el namespace para abrir la página
    free(pvActual);
    return ESP_FAIL;
  }
  return ESP_OK;
}

esp_err_t leer_pagina_variable(char **pname, char **pvActual,
                               int32_t *ContadorEntradaActual,
                               char **regActualPaginaVariable,
                               int32_t *valorEntradaActual) {
  /*	*** Leer_pagina_variable ***
   *	Esta función lee la pagina variable referenciada por leer_contador_pf()
   *para encontrar la entrada más reciente escrita y su valor.
   *
   *	Entradas:
   *		pvActual: Espacio de memoria >= sizeof(char)*5 para guardar el
   *namespace de la página actual regActualPaginaVariable: Espacio de memoria >=
   *sizeof(char)*5 para guardar el keyvalue de la entrada más reciente
   *		ContadorEntradaActual,valorEntradaActual: Punteros para ser
   *rellenados
   *
   *	Retorna:
   *		ContadorEntradaActual: Número que coincide con el
   *regActualPaginaVariable pero de naturaleza int32, permite construir el
   *namespace o modificarlo en otras funciones de ser necesario
   *		regActualPaginaVariable: Keyvalue de la entrada más reciente
   *		valorEntradaActual: valor guardado en la entrada más reciente,
   *representa el último conteó de pulsos registrado
   */

  nvs_handle_t my_handle;
  int32_t entradasEnPv = 0;

  // Buscando en toda la memoria
  nvs_iterator_t it = nvs_entry_find(*pname, *pvActual, NVS_TYPE_ANY);
  while (it != NULL) {
    entradasEnPv++;
    nvs_entry_info_t info;
    nvs_entry_info(it, &info);
    it = nvs_entry_next(it);
    printf("key '%s', type '%d' \n", info.key, info.type);
  };
  nvs_release_iterator(it);

  if (entradasEnPv == 0)
    entradasEnPv = 1;  // En caso de no encontrar nada en la página actual se
                       // asigna 1 para crear la entrada e1.

  *ContadorEntradaActual = entradasEnPv;

  // Creando el key del último registro encontrado
  if (asprintf(regActualPaginaVariable, "e%d", entradasEnPv) < 0) {
    free(regActualPaginaVariable);
    return ESP_FAIL;
  }

  // Abriendo la página variable
  esp_err_t err =
      nvs_open_from_partition(*pname, *pvActual, NVS_READWRITE, &my_handle);
  if (err != ESP_OK) return err;

  // Leyendo el último registro encontrado
  err = nvs_get_i32(my_handle, *regActualPaginaVariable, valorEntradaActual);
  if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    return err;
  else if (err == ESP_ERR_NVS_NOT_FOUND) {
    *valorEntradaActual = 0;  // Valor por defecto en lugar de no estar asignado
    // Write
    err = nvs_set_i32(my_handle, *regActualPaginaVariable, *valorEntradaActual);
    // Commit
    err = nvs_commit(my_handle);
    if (err != ESP_OK) return err;
  }
  // Close
  nvs_close(my_handle);

  ESP_LOGI("PV Last entry", "%d", *valorEntradaActual);
  if (*valorEntradaActual > 50000) {
    return ESP_FAIL;
  }
  return ESP_OK;
}

esp_err_t change_to_next_partition(char **pname, uint8_t *partition_number) {
  nvs_handle_t my_handle;
  esp_err_t err;
  char *aux;

  ESP_LOGI("PARTICION ACTUAL", "%s %d", *pname, *partition_number);

  // Abriendo particion y levantando la bandera
  err = nvs_open_from_partition(*pname, "storage", NVS_READWRITE, &my_handle);
  if (err != ESP_OK) {
    ESP_LOGE("NVS", "ERROR IN NVS_OPEN");
    return ESP_FAIL;
  } else {
    err = nvs_set_u8(my_handle, "finished", 1);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
      ESP_LOGE("NVS", "ERROR IN GET Finished Flag");
    err = nvs_commit(my_handle);
    if (err != ESP_OK) ESP_LOGE("NVS", "ERROR IN COMMIT");
  }
  // Close
  nvs_close(my_handle);

  ESP_LOGI("PARTICION ACTUAL", "DEBUG 0");

  free(*pname);

  ESP_LOGI("PARTICION ACTUAL", "DEBUG 1");

  if (*partition_number <= 3) {
    // Cerrando la particion anterior
    if (asprintf(&aux, "app%d", *partition_number - 1) < 0) {
      free(aux);
      ESP_LOGE("ROTAR_NVS", "Nombre de particion no fue creado");
    }
    printf("%s", aux);
    err = nvs_flash_deinit_partition(aux);
    if (err != ESP_OK)
      ESP_LOGE("CNP", "ERROR (%s) IN DEINIT", esp_err_to_name(err));
    else
      free(aux);

    // Inicializando la nueva partición
    if (asprintf(pname, "app%u", *partition_number) < 0) {
      free(*pname);
      ESP_LOGE("ROTAR_NVS", "Nombre de particion no fue creado");
    }
    ESP_LOGW("CNP", "Partition changed to %s", *pname);
    nvs_flash_init_partition(*pname);

    ESP_LOGI("PARTICION ACTUAL", "DEBUG 2 %s", *pname);

    // Llenando particion
    esp_err_t err =
        nvs_open_from_partition(*pname, "storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) ESP_LOGE("NVS", "ERROR IN NVS_OPEN");
    //		free(*pname);

    // Colocando la bandera de llenado en 0
    err = nvs_set_u8(my_handle, "finished", 0);
    if (err != ESP_OK) ESP_LOGE("NVS", "ERROR IN SET");
    err = nvs_commit(my_handle);
    if (err != ESP_OK) ESP_LOGE("NVS", "ERROR IN COMMIT");

    // Get del número de la partición
    err = nvs_get_u8(my_handle, "pnumber", partition_number);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
      ESP_LOGE("NVS", "ERROR IN GET");
    else if (err == ESP_ERR_NVS_NOT_FOUND) {
      err = nvs_set_u8(my_handle, "pnumber", *partition_number);
      if (err != ESP_OK) ESP_LOGE("NVS", "ERROR IN SET");
      err = nvs_commit(my_handle);
      if (err != ESP_OK) ESP_LOGE("NVS", "ERROR IN COMMIT");
    }
    // Close
    nvs_close(my_handle);
    return ESP_OK;
  } else {
    ESP_LOGE("APP", "All partitions full");
    return ESP_FAIL;
  }
}

esp_err_t levantar_bandera(char *pname) {
  nvs_handle_t my_handle;
  esp_err_t err;

  // Abriendo particion y levantando la bandera
  err = nvs_open_from_partition(pname, "storage", NVS_READWRITE, &my_handle);
  if (err != ESP_OK) {
    ESP_LOGE("NVS", "ERROR IN NVS_OPEN");
    return ESP_FAIL;
  } else {
    err = nvs_set_u8(my_handle, "finished", 1);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
      ESP_LOGE("NVS", "ERROR IN GET Finished Flag");
    err = nvs_commit(my_handle);
    if (err != ESP_OK) ESP_LOGE("NVS", "ERROR IN COMMIT");
  }
  // Close
  nvs_close(my_handle);

  ESP_LOGI("PARTICION ACTUAL", "DEBUG 1");

  return ESP_OK;
}

esp_err_t contar_pulsos_nvs(char **pname, uint8_t *partition_number,
                            int32_t *ContadorPvActual, int32_t *EntradaActual,
                            int32_t *CuentaPulsos) {
  /*	***Contar_pulsos_nvs***
   * 	Conteo de pulsos donde se aumenta la variables de numeración de pulsos,
   * entrada o página segun sea necesario.
   */

  char *key, *namespace;

  //	namespace = malloc(sizeof("pv##"));
  //	if(*namespace == 0x00) {
  //		free(namespace);
  //		return ESP_FAIL;
  //	}else sprintf(namespace, "pv%d", *ContadorPvActual);

  if (asprintf(&namespace, "pv%d", *ContadorPvActual) <
      0) {  // Aquí se crea el namespace para abrir la página
    free(namespace);
    return ESP_FAIL;
  }

  if (asprintf(&key, "e%d", *EntradaActual) < 0) {  // Aquí se crea el keyvalue
    free(key);
    return ESP_FAIL;
  }

  (*CuentaPulsos)++;

  if (*CuentaPulsos <= PPE_LIMIT) {
    guarda_cuenta_pulsos(*pname, namespace, key,
                         CuentaPulsos);  // Guarda cada pulso

  } else {
    (*EntradaActual)++;  // Límite de pulsos por entrada superado, cambiando a
                         // siguiente entrada
    if (*EntradaActual <= EPP_LIMIT) {
      *CuentaPulsos = 1;
      activa_entrada(*pname, namespace, &key, EntradaActual, CuentaPulsos);

    } else {
      (*ContadorPvActual)++;  // Límite de entradas por pagina superado se debe
                              // cambiar de página.
      if (*ContadorPvActual <= PPP_LIMIT) {
        *EntradaActual = 1;
        *CuentaPulsos = 1;
        abrir_pv(*pname, &namespace, &key, ContadorPvActual, EntradaActual,
                 CuentaPulsos);

      } else {
        if (*partition_number <= PARTITIONS_MAX) {
          *ContadorPvActual = 1;
          *EntradaActual = 1;
          *CuentaPulsos = 1;
        } else
          ESP_LOGE("PARTITIONS", "All partitions are full");
      }
    }
  }
  free(key);
  free(namespace);
  return ESP_OK;
}

esp_err_t search_init_partition(uint8_t *pnumber) {
  char *pname;
  nvs_stats_t info;
  nvs_handle_t my_handle;
  uint8_t partition_full;
  esp_err_t err;

  *pnumber = 0;

  for (uint8_t i = 1; i <= 3; i++) {
    if (asprintf(&pname, "app%d", i) < 0) {
      free(pname);
      ESP_LOGE("ROTAR_NVS", "Nombre de particion no fue creado");
      return ESP_FAIL;
    }

    ESP_LOGE("ROTAR_NVS", "%s", pname);
    err = nvs_flash_init_partition(pname);
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND ||
        gpio_get_level(GPIO_NUM_0) == 0) {
      // NVS partition was truncated and needs to be erased
      // Retry nvs_flash_init
      ESP_ERROR_CHECK(nvs_flash_erase_partition("app1"));
      ESP_ERROR_CHECK(nvs_flash_erase_partition("app2"));
      ESP_ERROR_CHECK(nvs_flash_erase_partition("app3"));
      ESP_ERROR_CHECK(nvs_flash_init_partition(pname));
    }

    err = nvs_get_stats(pname, &info);
    if (err == ESP_OK)
      ESP_LOGE("NVS INFO",
               "\n Total entries: %d\n Used entries:%d\n Free entries: %d\n "
               "Namespace count: %d",
               info.total_entries, info.used_entries, info.free_entries,
               info.namespace_count);

    // Revisando si la partición está llena
    esp_err_t err =
        nvs_open_from_partition(pname, "storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) ESP_LOGE("NVS", "ERROR IN NVS_OPEN");

    err = nvs_get_u8(my_handle, "finished", &partition_full);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
      ESP_LOGE("NVS", "ERROR (%s) IN GET", esp_err_to_name(err));
    else if (err == ESP_ERR_NVS_NOT_FOUND) {
      partition_full = 0;  // Valor por defecto en lugar de no estar asignado
      // Write
      err = nvs_set_u8(my_handle, "finished", partition_full);
      // Commit
      err = nvs_commit(my_handle);
      if (err != ESP_OK) ESP_LOGE("NVS", "ERROR IN COMMIT");
    }

    // Seteando el número de la partición
    err = nvs_get_u8(my_handle, "pnumber", pnumber);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
      ESP_LOGE("NVS", "ERROR IN GET");
    else if (err == ESP_ERR_NVS_NOT_FOUND) {
      // Write
      err = nvs_set_u8(my_handle, "pnumber", i);
      ESP_LOGW("SIP", "Partition number setted first time: %d", i);
      // Commit
      err = nvs_commit(my_handle);
      if (err != ESP_OK) ESP_LOGE("NVS", "ERROR IN COMMIT");
    } else {
      ESP_LOGW("DEBUG", "Partition number setted: %u", *pnumber);
    }

    // Close
    nvs_close(my_handle);
    nvs_flash_deinit_partition(pname);
    if (*pnumber == i) {
      ESP_LOGI("SIP", "OK");
    }

    if (partition_full == 1) {
      ESP_LOGW("SIP", "Partition number %d is full, trying with next", i);
      if (i == 3) {
        ESP_LOGE("SIP", "All partitions are full");
        return ESP_FAIL;
      }
    } else if (partition_full == 0) {
      ESP_LOGW("SIP", "NVS init can be done in partition app%d", i);
      *pnumber = i;
      break;
    }
  }
  return ESP_OK;
}

void task_pulse(void *arg) {
  CHECK_ERROR_CODE(esp_task_wdt_add(NULL), ESP_OK);
  CHECK_ERROR_CODE(esp_task_wdt_status(NULL), ESP_OK);
  ESP_LOGI(TAG, "Pulse counter task started");
  gpio_set_direction(GPIO_NUM_0, GPIO_MODE_INPUT);
  int pinLevel;
  /*** Inicialización de variables y ejecución de métodos para buscar la última
   * escritura***/
  uint8_t partition_number;  // Index de partition name (app#)
  int32_t indexPv,           // Index del namespace (pv#)
      indexEntradaActual,    // Index de la entrada activa en la pv (e#)
      cuentaPulsosPv;        // Cuenta de pulsos en la entrada activa
  uint64_t pulses = 0;
  char *pvActual,        // Namespace de la página variable
      *entradaActualpv,  // Key de la entrada actual
      *partition_name;   // Nombre de la partición
  bool counted = false;
  search_init_partition(&partition_number);
  if (asprintf(&partition_name, "app%d", partition_number) < 0) {
    free(partition_name);
    ESP_LOGE("ROTAR_NVS", "Nombre de particion no fue creado");
  }
  esp_err_t err = nvs_flash_init_partition(partition_name);
  // flash_get(&pulses);
  err = leer_contador_pf(&partition_name, &pvActual, &indexPv);
  if (err != ESP_OK)
    ESP_LOGE("ROTAR NVS", "Error (%s) static page counter",
             esp_err_to_name(err));
  err = leer_pagina_variable(&partition_name, &pvActual, &indexEntradaActual,
                             &entradaActualpv, &cuentaPulsosPv);
  if (err != ESP_OK)
    printf("Error (%s) buscando el registro escrito más reciente\n",
           esp_err_to_name(err));
  ESP_LOGI("LPV",
           "pvActual: %s indexEntradaActual: %d Entrada actual: %s Cuenta "
           "pulsos: %d",
           pvActual, indexEntradaActual, entradaActualpv, cuentaPulsosPv);
  /******************************************************************************************/
  while (1) {
    pinLevel = gpio_get_level(GPIO_NUM_0);
    if (pinLevel == 1 && counted == false) {
      /*** En caso de que se haya llenado una partición ***/
      if (indexPv == PPP_LIMIT && indexEntradaActual == EPP_LIMIT &&
          cuentaPulsosPv == PPE_LIMIT) {
        partition_number++;

        if (partition_number <= PARTITIONS_MAX) {
          levantar_bandera(partition_name);

          char *aux;
          nvs_handle_t my_handle;

          // Cerrando la particion anterior
          if (asprintf(&aux, "app%d", partition_number - 1) < 0) {
            free(aux);
            ESP_LOGE("ROTAR_NVS", "Nombre de particion no fue creado");
          }
          ESP_LOGI("DEINIT", "%s", aux);
          err = nvs_flash_deinit_partition(aux);
          if (err != ESP_OK)
            ESP_LOGE("CNP", "ERROR (%s) IN DEINIT", esp_err_to_name(err));
          else
            free(aux);

          // Inicializando la nueva partición
          if (asprintf(&partition_name, "app%u", partition_number) < 0) {
            free(partition_name);
            ESP_LOGE("ROTAR_NVS", "Nombre de particion no fue creado");
          }
          ESP_LOGW("CNP", "Partition changed to %s", partition_name);
          nvs_flash_init_partition(partition_name);

          ESP_LOGI("PARTICION ACTUAL", "DEBUG 2 %s", partition_name);

          // Llenando particion
          esp_err_t err = nvs_open_from_partition(partition_name, "storage",
                                                  NVS_READWRITE, &my_handle);
          if (err != ESP_OK) ESP_LOGE("NVS", "ERROR IN NVS_OPEN");

          // Colocando la bandera de llenado en 0
          err = nvs_set_u8(my_handle, "finished", 0);
          if (err != ESP_OK) ESP_LOGE("NVS", "ERROR IN SET");
          err = nvs_commit(my_handle);
          if (err != ESP_OK) ESP_LOGE("NVS", "ERROR IN COMMIT");

          // Get del número de la partición
          err = nvs_get_u8(my_handle, "pnumber", &partition_number);
          if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
            ESP_LOGE("NVS", "ERROR IN GET");
          else if (err == ESP_ERR_NVS_NOT_FOUND) {
            err = nvs_set_u8(my_handle, "pnumber", partition_number);
            if (err != ESP_OK) ESP_LOGE("NVS", "ERROR IN SET");
            err = nvs_commit(my_handle);
            if (err != ESP_OK) ESP_LOGE("NVS", "ERROR IN COMMIT");
          }
          // Close
          nvs_close(my_handle);
        }
      }
      /******************************************************************/

      /***** Aumenta pulsos *****/
      if (partition_number <= PARTITIONS_MAX) {
        err = contar_pulsos_nvs(&partition_name, &partition_number, &indexPv,
                                &indexEntradaActual, &cuentaPulsosPv);

        pulses =
            /*Asumiendo que inicia en cero por ahora*/
            // inicial_pulsos +  // Aporte de los pulsos iniciales
            (partition_number - 1) * PPP_LIMIT * EPP_LIMIT *
                PPE_LIMIT +                          // c/u de las particiones
            (indexPv - 1) * EPP_LIMIT * PPE_LIMIT +  // Páginas anteriores
            (indexEntradaActual - 1) * PPE_LIMIT +   // Página actual
            cuentaPulsosPv;                          // Entrada actual

        ESP_LOGI("PULSOS", "PARTICION: %d PV: %d ENT: %d Pulsos: %d",
                 partition_number, indexPv, indexEntradaActual, cuentaPulsosPv);
        ESP_LOGI("TOTAL PULSOS", "%llu", pulses);

        // Enviando los pulsos a otra tarea
        register_save(pulses, inputRegister);
      } else {
        ESP_LOGE("APP", "All partitions are full");
      }
      /******************************************************************/

      //    pulses++;
      //    flash_save(pulses);

      counted = true;
      ESP_LOGI(TAG, "Pulse number %llu", pulses);
    }
    if (pinLevel == 0 && counted == true) {
      counted = false;
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
    CHECK_ERROR_CODE(esp_task_wdt_reset(), ESP_OK);
  }
  free(pvActual);
  free(entradaActualpv);
  free(partition_name);
  ESP_LOGE("RTOS", "NVS-Rotative fall");
}
void task_modbus_slave(void *arg) {
  QueueHandle_t uart_queue;
  uart_event_t event;
  uint8_t *dtmp = (uint8_t *)malloc(RX_BUF_SIZE);
  modbus_registers[1] = &inputRegister[0];
  inputRegister[0] = 0x2324;
  inputRegister[1] = 0x2526;
  uart_init(&uart_queue);
  while (
      xQueueReceive(uart_queue, (void *)&event, (portTickType)portMAX_DELAY)) {
    bzero(dtmp, RX_BUF_SIZE);

    switch (event.type) {
      case UART_DATA:
        uart_read_bytes(UART_NUM_1, dtmp, event.size, portMAX_DELAY);
        ESP_LOGI(TAG, "Received data is:");
        for (int i = 0; i < event.size; i++) {
          printf("%x ", dtmp[i]);
        }
        printf("\n");
        if (CRC16(dtmp, event.size) == 0) {
          ESP_LOGI(TAG, "Modbus frame verified");
          if (dtmp[0] == CONFIG_ID) {
            ESP_LOGI(TAG, "Frame to this slave");
            modbus_slave_functions(dtmp, event.size, modbus_registers);
          }
        } else
          ESP_LOGI(TAG, "Frame not verified: %d", CRC16(dtmp, event.size));

        break;
      default:
        ESP_LOGI(TAG, "UART event");
    }
  }
  free(dtmp);
  dtmp = NULL;
}

void task_modbus_master(void *arg) {
  CHECK_ERROR_CODE(esp_task_wdt_add(NULL), ESP_OK);
  CHECK_ERROR_CODE(esp_task_wdt_status(NULL), ESP_OK);
  QueueHandle_t uart_queue;
  uart_event_t event;
  uint8_t *dtmp = (uint8_t *)malloc(RX_BUF_SIZE);
  modbus_registers[1] = &inputRegister[0];
  uart_init(&uart_queue);
  // const uint16_t start_add    = 0x0000;
  uint16_t quantity = 2;
  bool slaves[MAX_SLAVES + 1];
  init_slaves(slaves);
  uint8_t curr_slave = 1;
  if (CONFIG_SLAVES == 0) vTaskDelete(NULL);
  while (1) {
    read_input_register(curr_slave, (uint16_t)curr_slave, quantity);

    if (xQueuePeek(uart_queue, (void *)&event, (portTickType)10)) {
      if (event.type == UART_BREAK) {
        ESP_LOGI(TAG, "Cleaned");
        xQueueReset(uart_queue);
      }
    }
    if (xQueueReceive(uart_queue, (void *)&event, (portTickType)100)) {
      switch (event.type) {
        case UART_DATA:
          uart_read_bytes(UART_NUM_1, dtmp, event.size, portMAX_DELAY);
          ESP_LOGI(TAG, "Received data is:");
          for (int i = 0; i < event.size; i++) {
            printf("%x ", dtmp[i]);
          }
          printf("\n");
          if (CRC16(dtmp, event.size) == 0) {
            ESP_LOGI(TAG, "Modbus frame verified");
            save_register(dtmp, event.size, modbus_registers);
          } else
            ESP_LOGI(TAG, "Frame not verified: %d", CRC16(dtmp, event.size));
          break;
        default:
          ESP_LOGI(TAG, "uart event type: %d", event.type);
          break;
      }
    } else {
      ESP_LOGI(TAG, "Timeout");
      vTaskDelay(TIME_SCAN / portTICK_PERIOD_MS);
    }
    curr_slave++;
    if (curr_slave > CONFIG_SLAVES) curr_slave = 1;
    CHECK_ERROR_CODE(esp_task_wdt_reset(), ESP_OK);
  }
}
void task_lora(void *arg) {
  ESP_LOGI(TAG, "Task LoRa initialized");
  QueueHandle_t lora_queue;
  uart_lora_t config = {.uart_tx = 14, .uart_rx = 15, .baud_rate = 9600};

  config_rf1276_t config_mesh = {.baud_rate = 9600,
                                 .network_id = 1,
                                 .node_id = 2,
                                 .power = 7,
                                 .routing_time = 1,
                                 .freq = 433.0,
                                 .port_check = 0};

  lora_queue = xQueueCreate(1, 128);
  struct trama_receive trama;
  start_lora_mesh(config, config_mesh, &lora_queue);
  uint16_t node_origen;
  modbus_registers[1] = &inputRegister[0];
  // inputRegister[0]    = 0x2324;
  // inputRegister[1]    = 0x2526;
  mb_response_t modbus_response;
  struct send_data_struct data = {
      .node_id = 1, .power = 7, .data = {0}, .tamano = 1, .routing_type = 1};

  while (
      xQueueReceive(lora_queue, &trama.trama_rx, (portTickType)portMAX_DELAY)) {
    ESP_LOGI(TAG, "Receiving data...");
    if (receive_packet_rf1276(&trama) == 0) {
      node_origen = trama.respond.source_node.valor;
      printf("nodo origen es: %u\n", node_origen);
      modbus_response = modbus_lora_functions(
          trama.respond.data, trama.respond.data_length, modbus_registers);
      memcpy(data.data, modbus_response.frame, modbus_response.len);
      data.tamano = modbus_response.len;
      send_data_esp_rf1276(&data);
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
void app_main() {
  ESP_LOGI(TAG, "MCU initialized");
  ESP_LOGI(TAG, "Init Watchdog");
  CHECK_ERROR_CODE(esp_task_wdt_init(TWDT_TIMEOUT_S, true), ESP_OK);
#ifdef CONFIG_PULSE_PERIPHERAL
  ESP_LOGI(TAG, "Start peripheral");
  xTaskCreatePinnedToCore(task_pulse, "task_pulse", 1024 * 2, NULL, 10, NULL,
                          0);
#endif

#ifdef CONFIG_SLAVE_MODBUS
  ESP_LOGI(TAG, "Start Modbus slave task");
  xTaskCreatePinnedToCore(task_modbus_slave, "task_modbus_slave", 2048 * 2,
                          NULL, 10, NULL, 1);
#endif

#ifdef CONFIG_MASTER_MODBUS
  ESP_LOGI(TAG, "Start Modbus master task");
  xTaskCreatePinnedToCore(task_modbus_master, "task_modbus_master", 2048 * 2,
                          NULL, 10, NULL, 1);
#endif
#ifdef CONFIG_LORA
  ESP_LOGI(TAG, "Start LoRa task");
  xTaskCreatePinnedToCore(task_lora, "task_lora", 2048 * 4, NULL, 10, NULL, 1);
#endif
}
