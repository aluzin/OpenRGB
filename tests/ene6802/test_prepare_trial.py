# SPDX-License-Identifier: GPL-2.0-or-later
import copy
import unittest
import prepare_trial as trial

class TrialSettingsTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.names,cls.dimm=trial.registry_names()

    def test_valid_scopes(self):
        for addresses in ([0x71],[0x73],[0x71,0x73]):
            trial.verify(trial.config(self.names,addresses),self.names,self.dimm)

    def test_every_other_detector_disabled(self):
        original=trial.config(self.names,[0x71,0x73])
        for name in self.names - {trial.TARGET}:
            with self.subTest(name=name):
                changed=copy.deepcopy(original)
                changed['Detectors']['detectors'][name]=True
                with self.assertRaises(AssertionError): trial.verify(changed,self.names,self.dimm)

    def test_missing_dimm_guard(self):
        for name in self.dimm:
            changed=trial.config(self.names,[0x71,0x73])
            del changed['Detectors']['detectors'][name]
            with self.assertRaises(AssertionError): trial.verify(changed,self.names,self.dimm)

    def test_fingerprint_and_scope(self):
        changes=[('bus_name','PawnIO SMBus i801 1'),('address',0x77),
                 ('pci_subsystem_vendor',0),('pci_subsystem_device',0)]
        for field,value in changes:
            changed=trial.config(self.names,[0x71,0x73])
            changed['ENE6802Devices']['devices'][0][field]=value
            with self.assertRaises(AssertionError): trial.verify(changed,self.names,self.dimm)

    def test_reject_auto_profile_and_unlocked(self):
        changed=trial.config(self.names,[0x71,0x73])
        changed['ProfileManager']['startup_profile']['enabled']=True
        with self.assertRaises(AssertionError): trial.verify(changed,self.names,self.dimm)
        changed=trial.config(self.names,[0x71,0x73])
        changed['Drivers']['shared_smbus_access']=False
        with self.assertRaises(AssertionError): trial.verify(changed,self.names,self.dimm)

if __name__ == '__main__': unittest.main()
