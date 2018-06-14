'use strict'

const express = require('express');
const router = express.Router();
const Device = require('../models/deviceModel');
const Validation = require('../middlewares/validations/deviceValidation');

const TAG = 'DeviceController';

router.post('/register', Validation.validateDeviceRegistrationData, function(req, res, next){

	let device = new Device({
		mac_address : req.body.mac_address,
		name 		: req.body.name,
		user 		: req.body.userId,
		description : req.body.description,
		model		: req.body.modelName
	});

	device.register()
	.then(result => {
		if(result) res.status(200).send("Device registered successfully.");
		else {
			console.log(TAG + ' : ' +err);
			res.status(500).send({ error : "Internal server error"});
		}
	})
	.catch(err => {
		//Check if it is validation error
		if(err.name === 'ValidationError'){
			res.status(400).send(err);
		}
		//Unknown error, so pass it on  
		next(err);
	});

	return router;
});

router.get('/user/:id', function(req, res, next){

	let userId = req.params.id;

	if( !userId ){
		return res.status(400).send({error : "user id not provided"});
	}

	Device.fetchAllMappedToUserId(userId)
	.then(result => {
		if(result) res.status(200).send(result);
		else res.status(204).send({ message : 'Provided user id did not return any results.'});
	})
	.catch(err => {
		//Check if it is validation error
		if(err.name === 'ValidationError'){
			res.status(400).send(err);
		}
		//Unknown error, so pass it on  
		next(err);
	});

	return router;
});


router.get('/mac', Validation.validateMacAddress, function(req, res, next){

	let macAddress = req.query.macaddress;
	Device.fetchWithMacAddress(macAddress)
	.then(result => {
		if(result) res.status(200).send(result);
		else res.status(204).send({ message : 'Provided mac address did not return any results.'});
	})
	.catch(err => {
		//Check if it is validation error
		if(err.name === 'ValidationError'){
			res.status(400).send(err);
		}
		//Unknown error, so pass it on  
		next(err);
	});

	return router;
});

router.get('/:id', function(req, res, next){

	let deviceId = req.params.id;

	if( !deviceId || isNaN(parseInt(deviceId)) ){
		return res.status(400).send({error : "Invalid device id"});
	}

	Device.fetchWithDeviceId(deviceId)
	.then(result => {
		if(result) res.status(200).send(result);
		else res.status(204).send({ message : 'Provided device id did not return any results.'});
	})
	.catch(err => {
		//Check if it is validation error
		if(err.name === 'ValidationError'){
			res.status(400).send(err);
		}
		//Unknown error, so pass it on  
		next(err);
	});
	
	return router;
});


module.exports = router;